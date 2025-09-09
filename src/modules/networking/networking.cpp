/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */


#include "modules/networking/networking.hpp"

#include <boost/endian/conversion.hpp>
#include <libp2p/basic/read_varint.hpp>
#include <libp2p/basic/write_varint.hpp>
#include <libp2p/coro/spawn.hpp>
#include <libp2p/crypto/sha/sha256.hpp>
#include <libp2p/host/basic_host.hpp>
#include <libp2p/injector/host_injector.hpp>
#include <libp2p/protocol/gossip/gossip.hpp>
#include <libp2p/transport/quic/transport.hpp>
#include <qtils/to_shared_ptr.hpp>

#include "blockchain/block_tree.hpp"
#include "serde/serialization.hpp"
#include "serde/snappy.hpp"
#include "types/block_request.hpp"
#include "utils/__debug_env.hpp"
#include "utils/sample_peer.hpp"

namespace lean::modules {
  // TODO: gossip [from,seqno,signature,key]=None

  inline auto gossipTopic(std::string_view type) {
    return std::format("/leanconsensus/devnet0/{}/ssz_snappy", type);
  }

  auto encodeSszSnappy(const auto &t) {
    return snappyCompress(encode(t).value());
  }

  template <typename T>
  outcome::result<T> decodeSszSnappy(qtils::BytesIn compressed) {
    BOOST_OUTCOME_TRY(auto uncompressed, snappyUncompress(compressed));
    return decode<T>(uncompressed);
  }

  libp2p::protocol::gossip::MessageId gossipMessageId(
      const libp2p::protocol::gossip::Message &message) {
    constexpr qtils::ByteArr<4> MESSAGE_DOMAIN_INVALID_SNAPPY{0, 0, 0, 0};
    constexpr qtils::ByteArr<4> MESSAGE_DOMAIN_VALID_SNAPPY{1, 0, 0, 0};
    libp2p::crypto::Sha256 hasher;
    auto hash_topic = [&] {
      qtils::ByteArr<sizeof(uint64_t)> size;
      boost::endian::store_little_u64(size.data(), message.topic.size());
      hasher.write(size).value();
      hasher.write(message.topic).value();
    };
    if (auto uncompressed_res = snappyUncompress(message.data)) {
      auto &uncompressed = uncompressed_res.value();
      hash_topic();
      hasher.write(MESSAGE_DOMAIN_VALID_SNAPPY).value();
      hasher.write(uncompressed).value();
    } else {
      hasher.write(MESSAGE_DOMAIN_INVALID_SNAPPY).value();
      hash_topic();
      hasher.write(message.data).value();
    }
    auto hash = hasher.digest().value();
    hash.resize(20);
    return hash;
  }

  class StatusProtocol : public std::enable_shared_from_this<StatusProtocol>,
                         public libp2p::protocol::BaseProtocol {
   public:
    using GetStatus = std::function<StatusMessage()>;
    using OnStatus = std::function<void(messages::StatusMessageReceived)>;

    StatusProtocol(std::shared_ptr<boost::asio::io_context> io_context,
                   std::shared_ptr<libp2p::host::BasicHost> host,
                   GetStatus get_status,
                   OnStatus on_status)
        : io_context_{std::move(io_context)},
          host_{std::move(host)},
          get_status_{std::move(get_status)},
          on_status_{std::move(on_status)} {}

    // BaseProtocol
    libp2p::StreamProtocols getProtocolIds() const override {
      return {"/leanconsensus/req/status/1/ssz_snappy"};
    }
    void handle(std::shared_ptr<libp2p::Stream> stream) override {
      libp2p::coroSpawn(
          *io_context_,
          [self{shared_from_this()}, stream]() -> libp2p::Coro<void> {
            std::ignore = co_await self->coroHandle(stream);
          });
    }

    void start() {
      host_->listenProtocol(shared_from_this());
    }

    libp2p::CoroOutcome<void> connect(
        std::shared_ptr<libp2p::connection::CapableConnection> connection) {
      BOOST_OUTCOME_CO_TRY(
          auto stream, co_await host_->newStream(connection, getProtocolIds()));
      BOOST_OUTCOME_CO_TRY(co_await coroHandle(stream));
      co_return outcome::success();
    }

   private:
    libp2p::CoroOutcome<void> coroHandle(
        std::shared_ptr<libp2p::Stream> stream) {
      auto peer_id = stream->remotePeerId();
      BOOST_OUTCOME_CO_TRY(co_await libp2p::writeVarintMessage(
          stream, encodeSszSnappy(get_status_())));
      qtils::ByteVec encoded;
      BOOST_OUTCOME_CO_TRY(co_await libp2p::readVarintMessage(stream, encoded));
      BOOST_OUTCOME_CO_TRY(auto status,
                           decodeSszSnappy<StatusMessage>(encoded));
      on_status_(messages::StatusMessageReceived{
          .from_peer = peer_id,
          .notification = status,
      });
      co_return outcome::success();
    }

    std::shared_ptr<boost::asio::io_context> io_context_;
    std::shared_ptr<libp2p::host::BasicHost> host_;
    GetStatus get_status_;
    OnStatus on_status_;
  };

  class BlockRequestProtocol
      : public std::enable_shared_from_this<BlockRequestProtocol>,
        public libp2p::protocol::BaseProtocol {
   public:
    BlockRequestProtocol(std::shared_ptr<boost::asio::io_context> io_context,
                         std::shared_ptr<libp2p::host::BasicHost> host,
                         qtils::SharedRef<blockchain::BlockTree> block_tree)
        : io_context_{std::move(io_context)},
          host_{std::move(host)},
          block_tree_{std::move(block_tree)} {}

    // BaseProtocol
    libp2p::StreamProtocols getProtocolIds() const override {
      return {"/leanconsensus/req/blocks_by_root/1/ssz_snappy"};
    }
    void handle(std::shared_ptr<libp2p::Stream> stream) override {
      libp2p::coroSpawn(
          *io_context_,
          [self{shared_from_this()}, stream]() -> libp2p::Coro<void> {
            std::ignore = co_await self->coroRespond(stream);
          });
    }

    void start() {
      host_->listenProtocol(shared_from_this());
    }

    libp2p::CoroOutcome<BlockResponse> request(libp2p::PeerId peer_id,
                                               BlockRequest request) {
      BOOST_OUTCOME_CO_TRY(
          auto stream, co_await host_->newStream(peer_id, getProtocolIds()));
      BOOST_OUTCOME_CO_TRY(co_await libp2p::writeVarintMessage(
          stream, encodeSszSnappy(request)));
      qtils::ByteVec encoded;
      BOOST_OUTCOME_CO_TRY(co_await libp2p::readVarintMessage(stream, encoded));
      BOOST_OUTCOME_CO_TRY(auto response,
                           decodeSszSnappy<BlockResponse>(encoded));
      co_return response;
    }

   private:
    libp2p::CoroOutcome<void> coroRespond(
        std::shared_ptr<libp2p::Stream> stream) {
      qtils::ByteVec encoded;
      BOOST_OUTCOME_CO_TRY(co_await libp2p::readVarintMessage(stream, encoded));
      BOOST_OUTCOME_CO_TRY(auto request,
                           decodeSszSnappy<BlockRequest>(encoded));
      BlockResponse response;
      for (auto &block_hash : request.blocks) {
        BOOST_OUTCOME_CO_TRY(auto block,
                             block_tree_->tryGetSignedBlock(block_hash));
        if (block.has_value()) {
          response.blocks.push_back(std::move(block.value()));
        }
      }
      BOOST_OUTCOME_CO_TRY(co_await libp2p::writeVarintMessage(
          stream, encodeSszSnappy(response)));
      co_return outcome::success();
    }

    std::shared_ptr<boost::asio::io_context> io_context_;
    std::shared_ptr<libp2p::host::BasicHost> host_;
    qtils::SharedRef<blockchain::BlockTree> block_tree_;
  };

  NetworkingImpl::NetworkingImpl(
      NetworkingLoader &loader,
      qtils::SharedRef<log::LoggingSystem> logging_system,
      qtils::SharedRef<blockchain::BlockTree> block_tree)
      : loader_(loader),
        logger_(logging_system->getLogger("Networking", "networking_module")),
        block_tree_{std::move(block_tree)} {
    libp2p::log::setLoggingSystem(logging_system->getSoralog());
  }

  NetworkingImpl::~NetworkingImpl() {
    if (io_thread_.has_value()) {
      io_context_->stop();
      io_thread_->join();
    }
  }

  void NetworkingImpl::on_loaded_success() {
    SL_INFO(logger_, "Loaded success");

    SamplePeer sample_peer{getPeerIndex()};

    auto injector = qtils::toSharedPtr(libp2p::injector::makeHostInjector(
        libp2p::injector::useKeyPair(sample_peer.keypair),
        libp2p::injector::useTransportAdaptors<
            libp2p::transport::QuicTransport>()));
    injector_ = injector;
    io_context_ = injector->create<std::shared_ptr<boost::asio::io_context>>();

    auto host = injector->create<std::shared_ptr<libp2p::host::BasicHost>>();

    if (auto r = host->listen(sample_peer.listen); not r.has_value()) {
      SL_WARN(logger_, "listen {} error: {}", sample_peer.listen, r.error());
    }
    host->start();

    auto on_peer_connected =
        [weak_self{weak_from_this()}](
            std::weak_ptr<libp2p::connection::CapableConnection>
                weak_connection) {
          auto connection = weak_connection.lock();
          if (not connection) {
            return;
          }
          auto self = weak_self.lock();
          if (not self) {
            return;
          }
          auto peer_id = connection->remotePeer();
          self->loader_.dispatch_peer_connected(
              qtils::toSharedPtr(messages::PeerConnectedMessage{peer_id}));
          if (connection->isInitiator()) {
            libp2p::coroSpawn(
                *self->io_context_,
                [status_protocol{self->status_protocol_},
                 connection]() -> libp2p::Coro<void> {
                  std::ignore = co_await status_protocol->connect(connection);
                });
          }
        };
    auto on_peer_disconnected =
        [weak_self{weak_from_this()}](libp2p::PeerId peer_id) {
          auto self = weak_self.lock();
          if (not self) {
            return;
          }
          self->loader_.dispatch_peer_disconnected(
              qtils::toSharedPtr(messages::PeerDisconnectedMessage{peer_id}));
        };
    on_peer_connected_sub_ =
        host->getBus()
            .getChannel<libp2p::event::network::OnNewConnectionChannel>()
            .subscribe(on_peer_connected);
    on_peer_disconnected_sub_ =
        host->getBus()
            .getChannel<libp2p::event::network::OnPeerDisconnectedChannel>()
            .subscribe(on_peer_disconnected);

    status_protocol_ = std::make_shared<StatusProtocol>(
        io_context_,
        host,
        [block_tree{block_tree_}]() { return block_tree->getStatusMessage(); },
        [weak_self{weak_from_this()}](messages::StatusMessageReceived message) {
          auto self = weak_self.lock();
          if (not self) {
            return;
          }
          self->receiveStatus(message);
        });
    status_protocol_->start();

    block_request_protocol_ =
        std::make_shared<BlockRequestProtocol>(io_context_, host, block_tree_);
    block_request_protocol_->start();

    gossip_ =
        injector->create<std::shared_ptr<libp2p::protocol::gossip::Gossip>>();
    gossip_->start();

    gossip_blocks_topic_ = gossipSubscribe<SignedBlock>(
        "block", [weak_self{weak_from_this()}](SignedBlock &&block) {
          auto self = weak_self.lock();
          if (not self) {
            return;
          }
          block.message.setHash();
          self->receiveBlock(std::nullopt, std::move(block));
        });
    gossip_votes_topic_ = gossipSubscribe<SignedVote>(
        "vote", [weak_self{weak_from_this()}](SignedVote &&vote) {
          auto self = weak_self.lock();
          if (not self) {
            return;
          }
          self->loader_.dispatch_SignedVoteReceived(
              std::make_shared<messages::SignedVoteReceived>(std::move(vote)));
        });

    if (sample_peer.index != 0) {
      libp2p::coroSpawn(
          *io_context_,
          [weak_self{weak_from_this()}, host]() -> libp2p::Coro<void> {
            SamplePeer sample_peer{0};

            if (auto r = co_await host->connect(sample_peer.connect_info);
                not r.has_value()) {
              auto self = weak_self.lock();
              if (not self) {
                co_return;
              }
              SL_WARN(self->logger_,
                      "connect {} error: {}",
                      sample_peer.connect,
                      r.error());
            }
          });
    }

    io_thread_.emplace([io_context{io_context_}] {
      auto work_guard = boost::asio::make_work_guard(*io_context);
      io_context->run();
    });
  }

  void NetworkingImpl::on_loading_is_finished() {
    SL_INFO(logger_, "Loading is finished");
  }

  void NetworkingImpl::on_dispatch_SendSignedBlock(
      std::shared_ptr<const messages::SendSignedBlock> message) {
    boost::asio::post(*io_context_, [self{shared_from_this()}, message] {
      self->gossip_blocks_topic_->publish(
          encodeSszSnappy(message->notification));
    });
  }

  void NetworkingImpl::on_dispatch_SendSignedVote(
      std::shared_ptr<const messages::SendSignedVote> message) {
    boost::asio::post(*io_context_, [self{shared_from_this()}, message] {
      self->gossip_votes_topic_->publish(
          encodeSszSnappy(message->notification));
    });
  }

  template <typename T>
  std::shared_ptr<libp2p::protocol::gossip::Topic>
  NetworkingImpl::gossipSubscribe(std::string_view type, auto f) {
    auto topic = gossip_->subscribe(gossipTopic(type));
    libp2p::coroSpawn(*io_context_,
                      [topic, f{std::move(f)}]() -> libp2p::Coro<void> {
                        while (auto raw_result = co_await topic->receive()) {
                          auto &raw = raw_result.value();
                          if (auto uncompressed_res = snappyUncompress(raw)) {
                            auto &uncompressed = uncompressed_res.value();
                            if (auto r = decode<T>(uncompressed)) {
                              f(std::move(r.value()));
                            }
                          }
                        }
                      });
    return topic;
  }

  void NetworkingImpl::receiveStatus(
      const messages::StatusMessageReceived &message) {
    BlockIndex finalized{
        message.notification.finalized.slot,
        message.notification.finalized.root,
    };
    BlockIndex head{
        message.notification.head.slot,
        message.notification.head.root,
    };
    if (not statusFinalizedIsGood(finalized)
        or not statusFinalizedIsGood(head)) {
      return;
    }
    if (not block_cache_.contains(head.hash) and not block_tree_->has(head.hash)
        and head.slot > block_tree_->lastFinalized().slot) {
      SL_TRACE(logger_, "receiveStatus {} => request", head.slot);
      requestBlock(message.from_peer, head.hash);
    }
    loader_.dispatch_StatusMessageReceived(qtils::toSharedPtr(message));
  }

  void NetworkingImpl::requestBlock(const libp2p::PeerId &peer_id,
                                    const BlockHash &block_hash) {
    libp2p::coroSpawn(*io_context_,
                      [self{shared_from_this()},
                       peer_id,
                       block_hash]() -> libp2p::Coro<void> {
                        auto response_res =
                            co_await self->block_request_protocol_->request(
                                peer_id, {.blocks = {{block_hash}}});
                        if (response_res.has_value()) {
                          auto &blocks = response_res.value().blocks.data();
                          if (blocks.empty()) {
                            co_return;
                          }
                          auto &block = blocks.at(0);
                          block.message.setHash();
                          self->receiveBlock(peer_id, std::move(block));
                        }
                      });
  }

  // TODO: detect finalized change
  void NetworkingImpl::receiveBlock(std::optional<libp2p::PeerId> from_peer,
                                    SignedBlock &&block) {
    auto slot_hash = block.message.slotHash();
    SL_TRACE(logger_, "receiveBlock {}", slot_hash.slot);
    auto remove = [&](auto f) {
      std::vector<BlockHash> queue{slot_hash.hash};
      while (not queue.empty()) {
        auto hash = queue.back();
        queue.pop_back();
        auto [begin, end] = block_children_.equal_range(hash);
        for (auto it = begin; it != end; it = block_children_.erase(it)) {
          f(it->second);
          queue.emplace_back(it->second);
        }
      }
    };
    auto parent_hash = block.message.parent_root;
    if (block_cache_.contains(slot_hash.hash)) {
      SL_TRACE(logger_, "receiveBlock {} => ignore cached", slot_hash.slot);
      return;
    }
    if (block_tree_->has(slot_hash.hash)) {
      SL_TRACE(logger_, "receiveBlock {} => ignore db", slot_hash.slot);
      return;
    }
    if (slot_hash.slot <= block_tree_->lastFinalized().slot) {
      SL_TRACE(
          logger_, "receiveBlock {} => ignore finalized fork", slot_hash.slot);
      remove(
          [&](const BlockHash &block_hash) { block_cache_.erase(block_hash); });
      return;
    }
    if (block_tree_->has(parent_hash)) {
      std::vector<SignedBlock> blocks{std::move(block)};
      remove([&](const BlockHash &block_hash) {
        blocks.emplace_back(block_cache_.extract(block_hash).mapped());
      });
      std::string __s;
      for (auto &block : blocks) {
        __s += std::format(" {}", block.message.slot);
      }
      SL_TRACE(logger_, "receiveBlock {} => import{}", slot_hash.slot, __s);
      block_tree_->import(std::move(blocks));
      return;
    }
    block_cache_.emplace(slot_hash.hash, std::move(block));
    block_children_.emplace(parent_hash, slot_hash.hash);
    if (block_cache_.contains(parent_hash)) {
      SL_TRACE(logger_, "receiveBlock {} => has parent", slot_hash.slot);
      return;
    }
    if (not from_peer) {
      return;
    }
    requestBlock(from_peer.value(), parent_hash);
    SL_TRACE(logger_, "receiveBlock {} => request parent", slot_hash.slot);
  }

  bool NetworkingImpl::statusFinalizedIsGood(const BlockIndex &slot_hash) {
    if (auto expected = block_tree_->getNumberByHash(slot_hash.hash)) {
      return slot_hash.slot == expected.value();
    }
    return slot_hash.slot > block_tree_->lastFinalized().slot;
  }
}  // namespace lean::modules
