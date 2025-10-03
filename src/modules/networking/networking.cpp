/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */


#include "modules/networking/networking.hpp"

#include <format>
#include <memory>
#include <stdexcept>

#include <app/configuration.hpp>
#include <blockchain/fork_choice.hpp>
#include <boost/endian/conversion.hpp>
#include <libp2p/basic/read_varint.hpp>
#include <libp2p/basic/write_varint.hpp>
#include <libp2p/coro/spawn.hpp>
#include <libp2p/crypto/key_marshaller.hpp>
#include <libp2p/crypto/sha/sha256.hpp>
#include <libp2p/host/basic_host.hpp>
#include <libp2p/injector/host_injector.hpp>
#include <libp2p/peer/identity_manager.hpp>
#include <libp2p/protocol/gossip/gossip.hpp>
#include <libp2p/transport/quic/transport.hpp>
#include <qtils/to_shared_ptr.hpp>

#include "blockchain/block_tree.hpp"
#include "blockchain/impl/fc_block_tree.hpp"
#include "modules/networking/block_request_protocol.hpp"
#include "modules/networking/get_node_key.hpp"
#include "modules/networking/ssz_snappy.hpp"
#include "modules/networking/status_protocol.hpp"
#include "modules/networking/types.hpp"

namespace lean::modules {
  // TODO(turuslan): gossip [from,seqno,signature,key]=None

  inline auto gossipTopic(std::string_view type) {
    return std::format("/leanconsensus/devnet0/{}/ssz_snappy", type);
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

  NetworkingImpl::NetworkingImpl(
      NetworkingLoader &loader,
      qtils::SharedRef<log::LoggingSystem> logging_system,
      qtils::SharedRef<blockchain::BlockTree> block_tree,
      qtils::SharedRef<lean::ForkChoiceStore> fork_choice_store,
      qtils::SharedRef<app::ChainSpec> chain_spec,
      qtils::SharedRef<app::Configuration> config,
      qtils::SharedRef<ValidatorRegistry> validator_registry)
      : loader_(loader),
        logger_(logging_system->getLogger("Networking", "networking_module")),
        block_tree_{std::move(block_tree)},
        fork_choice_store_{std::move(fork_choice_store)},
        chain_spec_{std::move(chain_spec)},
        config_{std::move(config)},
        validator_registry_{std::move(validator_registry)} {
    libp2p::log::setLoggingSystem(logging_system->getSoralog());
    block_tree_ = std::make_shared<blockchain::FCBlockTree>(fork_choice_store_);
  }

  NetworkingImpl::~NetworkingImpl() {
    if (io_thread_.has_value()) {
      io_context_->stop();
      io_thread_->join();
    }
  }

  void NetworkingImpl::on_loaded_success() {
    SL_INFO(logger_, "Loaded success");

    // Determine the keypair: from config if valid, otherwise random
    libp2p::crypto::KeyPair keypair;
    if (auto &node_key_hex = config_->nodeKeyHex(); node_key_hex.has_value()) {
      auto keypair_res = keyPairFromPrivateKeyHex(*node_key_hex);
      if (keypair_res.has_value()) {
        keypair = std::move(keypair_res.value());
      } else {
        SL_CRITICAL(logger_,
                    "Failed to parse node key from --node-key: {}, generating "
                    "a random one",
                    keypair_res.error().message());
        keypair = randomKeyPair();
      }
    } else {
      keypair = randomKeyPair();
    }

    // Always set up identity and peer info
    libp2p::peer::IdentityManager identity_manager{
        keypair,
        std::make_shared<libp2p::crypto::marshaller::KeyMarshaller>(nullptr)};
    auto peer_id = identity_manager.getId();

    SL_INFO(logger_, "Networking loaded with PeerId {}", peer_id.toBase58());

    auto injector = qtils::toSharedPtr(libp2p::injector::makeHostInjector(
        libp2p::injector::useKeyPair(keypair),
        libp2p::injector::useTransportAdaptors<
            libp2p::transport::QuicTransport>()));
    injector_ = injector;
    io_context_ = injector->create<std::shared_ptr<boost::asio::io_context>>();

    auto host = injector->create<std::shared_ptr<libp2p::host::BasicHost>>();

    if (auto &listen_addr = config_->listenMultiaddr();
        listen_addr.has_value()) {
      auto listen_res = libp2p::multi::Multiaddress::create(*listen_addr);
      if (listen_res.has_value()) {
        auto listen = listen_res.value();
        if (auto r = host->listen(listen); not r.has_value()) {
          SL_INFO(logger_,
                  "Listening address configured via --listen-addr: {}",
                  listen);
        } else {
          SL_INFO(logger_, "Listening on {}", listen);
        }
      } else {
        SL_CRITICAL(logger_,
                    "Invalid listen multiaddress '{}': {}",
                    *listen_addr,
                    listen_res.error().message());
        std::exit(1);
      }
    } else {
      SL_WARN(logger_, "No listen multiaddress configured");
    }
    host->start();

    // Add bootnodes from chain spec
    const auto &bootnodes = chain_spec_->getBootnodes();
    if (!bootnodes.empty()) {
      SL_INFO(logger_,
              "Adding {} bootnodes to address repository",
              bootnodes.size());

      auto &address_repo = host->getPeerRepository().getAddressRepository();

      for (const auto &bootnode : bootnodes.getBootnodes()) {
        if (bootnode.peer_id == peer_id) {
          continue;
        }
        std::vector<libp2p::multi::Multiaddress> addresses{bootnode.address};

        // Add bootnode addresses with permanent TTL
        auto result = address_repo.upsertAddresses(
            bootnode.peer_id,
            std::span<const libp2p::multi::Multiaddress>(addresses),
            libp2p::peer::ttl::kPermanent);

        if (result.has_value()) {
          SL_DEBUG(logger_,
                   "Added bootnode: peer={}, address={}",
                   bootnode.peer_id,
                   bootnode.address.getStringAddress());
        } else {
          SL_WARN(logger_,
                  "Failed to add bootnode: peer={}, error={}",
                  bootnode.peer_id,
                  result.error());
        }
        libp2p::PeerInfo peer_info{.id = bootnode.peer_id,
                                   .addresses = addresses};
        libp2p::coroSpawn(*io_context_,
                          [weak_self{weak_from_this()},
                           host,
                           peer_info]() -> libp2p::Coro<void> {
                            if (auto r = co_await host->connect(peer_info);
                                not r.has_value()) {
                              auto self = weak_self.lock();
                              if (not self) {
                                co_return;
                              }
                              SL_WARN(self->logger_,
                                      "connect {} error: {}",
                                      peer_info.id,
                                      r.error());
                            }
                          });
      }
    } else {
      SL_DEBUG(logger_, "No bootnodes configured");
    }

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
        [block_tree{block_tree_}]() {
          auto finalized = block_tree->lastFinalized();
          auto head = block_tree->bestBlock();
          return StatusMessage{
              .finalized = {.root = finalized.hash, .slot = finalized.slot},
              .head = {.root = head.hash, .slot = head.slot},
          };
        },
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
        "vote", [weak_self{weak_from_this()}](SignedVote &&signed_vote) {
          auto self = weak_self.lock();
          if (not self) {
            return;
          }
          auto res =
              self->fork_choice_store_->processAttestation(signed_vote, false);
          if (not res.has_value()) {
            SL_WARN(self->logger_,
                    "Error processing vote for target {}@{}: {}",
                    signed_vote.data.target.slot,
                    signed_vote.data.target.root,
                    res.error());
            return;
          }
          SL_INFO(self->logger_,
                  "Received vote for target {}@{}",
                  signed_vote.data.target.slot,
                  signed_vote.data.target.root);
        });

    io_thread_.emplace([io_context{io_context_}] {
      auto work_guard = boost::asio::make_work_guard(*io_context);
      io_context->run();
    });
  }

  void NetworkingImpl::on_loading_is_finished() {
    SL_INFO(logger_, "Loading is finished");
  }

  void NetworkingImpl::onSendSignedBlock(
      std::shared_ptr<const messages::SendSignedBlock> message) {
    boost::asio::post(*io_context_, [self{shared_from_this()}, message] {
      self->gossip_blocks_topic_->publish(
          encodeSszSnappy(message->notification));
    });
  }

  void NetworkingImpl::onSendSignedVote(
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
    libp2p::coroSpawn(
        *io_context_,
        [this, type, topic, f{std::move(f)}]() -> libp2p::Coro<void> {
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
    loader_.dispatchStatusMessageReceived(qtils::toSharedPtr(message));
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

  // TODO(turuslan): detect finalized change
  void NetworkingImpl::receiveBlock(std::optional<libp2p::PeerId> from_peer,
                                    SignedBlock &&signed_block) {
    auto slot_hash = signed_block.message.slotHash();
    SL_DEBUG(logger_,
             "receiveBlock slot {} hash {}",
             slot_hash.slot,
             slot_hash.hash);
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
    auto parent_hash = signed_block.message.parent_root;
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
      std::vector<SignedBlock> blocks{std::move(signed_block)};
      remove([&](const BlockHash &block_hash) {
        blocks.emplace_back(block_cache_.extract(block_hash).mapped());
      });
      std::string __s;
      for (auto &block : blocks) {
        __s += std::format(" {}", block.message.slot);
        auto res = fork_choice_store_->onBlock(block.message);
        if (not res.has_value()) {
          SL_WARN(logger_,
                  "Error importing block {}@{}: {}",
                  block.message.hash(),
                  block.message.slot,
                  res.error());
          return;
        }
      }
      SL_INFO(logger_, "receiveBlock {} => import{}", slot_hash.slot, __s);
      block_tree_->import(std::move(blocks));
      return;
    }
    block_cache_.emplace(slot_hash.hash, std::move(signed_block));
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
