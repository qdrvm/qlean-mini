/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */


#include "modules/networking/networking.hpp"

#include <algorithm>
#include <format>
#include <memory>
#include <stdexcept>

#include <app/configuration.hpp>
#include <blockchain/fork_choice.hpp>
#include <boost/endian/conversion.hpp>
#include <libp2p/basic/read_varint.hpp>
#include <libp2p/basic/write_varint.hpp>
#include <libp2p/coro/spawn.hpp>
#include <libp2p/coro/timer_loop.hpp>
#include <libp2p/crypto/key_marshaller.hpp>
#include <libp2p/crypto/sha/sha256.hpp>
#include <libp2p/host/basic_host.hpp>
#include <libp2p/injector/host_injector.hpp>
#include <libp2p/peer/identity_manager.hpp>
#include <libp2p/protocol/gossip/gossip.hpp>
#include <libp2p/protocol/identify.hpp>
#include <libp2p/protocol/ping.hpp>
#include <libp2p/transport/quic/transport.hpp>
#include <libp2p/transport/tcp/tcp_util.hpp>
#include <qtils/to_shared_ptr.hpp>

#include "blockchain/block_tree.hpp"
#include "blockchain/impl/fc_block_tree.hpp"
#include "modules/networking/block_request_protocol.hpp"
#include "modules/networking/ssz_snappy.hpp"
#include "modules/networking/status_protocol.hpp"
#include "modules/networking/types.hpp"

namespace lean::modules {
  constexpr std::chrono::seconds kConnectToPeersTimer{5};
  constexpr std::chrono::milliseconds kInitBackoff = std::chrono::seconds{10};
  constexpr std::chrono::milliseconds kMaxBackoff = std::chrono::minutes{5};

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
      qtils::SharedRef<app::Configuration> config)
      : loader_(loader),
        logger_(logging_system->getLogger("Networking", "networking_module")),
        block_tree_{std::move(block_tree)},
        fork_choice_store_{std::move(fork_choice_store)},
        chain_spec_{std::move(chain_spec)},
        config_{std::move(config)},
        random_{std::random_device{}()} {
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

    auto keypair = config_->nodeKey();

    // Always set up identity and peer info
    libp2p::peer::IdentityManager identity_manager{
        keypair,
        std::make_shared<libp2p::crypto::marshaller::KeyMarshaller>(nullptr)};
    auto peer_id = identity_manager.getId();

    SL_INFO(logger_, "Networking loaded with PeerId={}", peer_id.toBase58());

    libp2p::protocol::gossip::Config gossip_config;
    gossip_config.validation_mode =
        libp2p::protocol::gossip::ValidationMode::Anonymous;
    gossip_config.message_authenticity =
        libp2p::protocol::gossip::MessageAuthenticity::Anonymous;
    gossip_config.message_id_fn = gossipMessageId;
    gossip_config.protocol_versions.erase(
        libp2p::protocol::gossip::kProtocolGossipsubv1_2);

    auto injector = qtils::toSharedPtr(libp2p::injector::makeHostInjector(
        libp2p::injector::useKeyPair(keypair),
        libp2p::injector::useGossipConfig(std::move(gossip_config)),
        libp2p::injector::useTransportAdaptors<
            libp2p::transport::QuicTransport>()));
    injector_ = injector;
    io_context_ = injector->create<std::shared_ptr<boost::asio::io_context>>();

    auto host = injector->create<std::shared_ptr<libp2p::host::BasicHost>>();
    host_ = host;

    bool has_enr_listen_address = false;
    const auto &bootnodes = chain_spec_->getBootnodes();
    for (auto &bootnode : bootnodes.getBootnodes()) {
      if (bootnode.peer_id != peer_id) {
        continue;
      }
      has_enr_listen_address = true;
      auto info_res = libp2p::transport::detail::asQuic(bootnode.address);
      if (not info_res.has_value()) {
        SL_WARN(logger_, "Incompatible enr address {}", bootnode.address);
        continue;
      }
      auto port = info_res.value().port;
      auto enr_listen_address =
          libp2p::multi::Multiaddress::create(
              std::format("/ip4/0.0.0.0/udp/{}/quic-v1", port))
              .value();
      if (auto r = host->listen(enr_listen_address); not r.has_value()) {
        SL_WARN(logger_,
                "Error listening on enr address {}: {}",
                enr_listen_address,
                r.error());
      } else {
        SL_INFO(logger_, "Listening on {}", enr_listen_address);
      }
    }

    if (auto &listen_addr = config_->listenMultiaddr();
        listen_addr.has_value()) {
      auto &listen = *listen_addr;
      if (auto r = host->listen(listen); not r.has_value()) {
        SL_WARN(logger_,
                "Listening address configured via --listen-addr: {}",
                listen);
      } else {
        SL_INFO(logger_, "Listening on {}", listen);
      }
    } else if (not has_enr_listen_address) {
      SL_WARN(logger_, "No listen multiaddress configured");
    }
    host->start();

    // Add bootnodes from chain spec
    if (!bootnodes.empty()) {
      SL_INFO(logger_, "Found {} bootnodes in chain spec", bootnodes.size());

      auto &address_repo = host->getPeerRepository().getAddressRepository();

      // Collect candidates (exclude ourselves)
      auto all_bootnodes = bootnodes.getBootnodes();
      connectable_peers_.reserve(all_bootnodes.size());
      for (auto &bootnode : all_bootnodes) {
        if (bootnode.peer_id == peer_id) {
          continue;
        }
        connectable_peers_.emplace_back(bootnode.peer_id);
        peer_states_.emplace(
            bootnode.peer_id,
            PeerState{
                .info =
                    {
                        .id = bootnode.peer_id,
                        .addresses = {bootnode.address},
                    },
                .state = PeerState::Connectable{.backoff = kInitBackoff},
            });
      }

      SL_INFO(logger_,
              "Adding {} bootnodes to address repository",
              connectable_peers_.size());

      for (const auto &bootnode : all_bootnodes) {
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
      }
      libp2p::timerLoop(
          *io_context_, kConnectToPeersTimer, [weak_self{weak_from_this()}] {
            auto self = weak_self.lock();
            if (not self) {
              return false;
            }
            self->connectToPeers();
            return true;
          });
    } else {
      SL_DEBUG(logger_, "No bootnodes configured");
    }

    // Restore peer connection handlers and protocol startup
    auto on_peer_connected = [host, weak_self{weak_from_this()}](
                                 std::weak_ptr<
                                     libp2p::connection::CapableConnection>
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
      auto state_it = self->peer_states_.find(peer_id);
      if (state_it != self->peer_states_.end()) {
        auto &state = state_it->second;
        if (not std::holds_alternative<PeerState::Connected>(state.state)) {
          if (std::holds_alternative<PeerState::Connectable>(state.state)) {
            // Connectable => Connected
            auto connectable_it =
                std::ranges::find(self->connectable_peers_, peer_id);
            if (connectable_it == self->connectable_peers_.end()) {
              throw std::logic_error{
                  "inconsistent connectable_peers_ and peer_states_"};
            }
            self->connectable_peers_.erase(connectable_it);
          }
          // Connectable | Connecting | Backoff => Connected
          state.state = PeerState::Connected{};
        }
      }
      self->loader_.dispatch_peer_connected(
          qtils::toSharedPtr(messages::PeerConnectedMessage{peer_id}));
      if (connection->isInitiator()) {
        libp2p::coroSpawn(*self->io_context_,
                          [status_protocol{self->status_protocol_},
                           connection]() -> libp2p::Coro<void> {
                            std::ignore =
                                co_await status_protocol->connect(connection);
                          });
      } else {
        // Non-initiator: record remote address in peer repository
        auto addr_res = connection->remoteMultiaddr();
        if (!addr_res.has_value()) {
          SL_WARN(
              self->logger_, "remoteMultiaddr() failed: {}", addr_res.error());
          return;
        }

        std::vector<libp2p::multi::Multiaddress> addrs;
        addrs.emplace_back(addr_res.value());

        if (auto result =
                host->getPeerRepository().getAddressRepository().addAddresses(
                    peer_id,
                    std::span<const libp2p::multi::Multiaddress>(addrs),
                    libp2p::peer::ttl::kRecentlyConnected);
            not result.has_value()) {
          SL_WARN(self->logger_,
                  "Failed to add addresses for peer={}: {}",
                  peer_id,
                  result.error());
        }
      }
    };

    auto on_peer_disconnected =
        [weak_self{weak_from_this()}](libp2p::PeerId peer_id) {
          auto self = weak_self.lock();
          if (not self) {
            return;
          }
          auto state_it = self->peer_states_.find(peer_id);
          if (state_it != self->peer_states_.end()) {
            auto &state = state_it->second;
            if (std::holds_alternative<PeerState::Connected>(state.state)) {
              auto backoff = kInitBackoff;
              // Connected => Backoff
              state.state = PeerState::Backoff{
                  .backoff = backoff,
                  .backoff_until = Clock::now() + backoff,
              };
            }
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
    ping_ = injector->create<std::shared_ptr<libp2p::protocol::Ping>>();
    identify_ = injector->create<std::shared_ptr<libp2p::protocol::Identify>>();

    gossip_->start();
    ping_->start();
    identify_->start();

    gossip_blocks_topic_ = gossipSubscribe<SignedBlockWithAttestation>(
        "block",
        [weak_self{weak_from_this()}](
            SignedBlockWithAttestation &&signed_block_with_attestation,
            std::optional<libp2p::PeerId> received_from) {
          auto self = weak_self.lock();
          if (not self) {
            return;
          }
          signed_block_with_attestation.message.block.setHash();
          self->receiveBlock(received_from,
                             std::move(signed_block_with_attestation));
        });
    gossip_votes_topic_ = gossipSubscribe<SignedAttestation>(
        "attestation",
        [weak_self{weak_from_this()}](SignedAttestation &&signed_attestation,
                                      std::optional<libp2p::PeerId> peer_id) {
          auto self = weak_self.lock();
          if (not self) {
            return;
          }

          SL_DEBUG(self->logger_,
                   "Received vote for target={} 🗳️ from peer={} 👤 "
                   "validator_id={} ✅",
                   signed_attestation.message.data.target,
                   peer_id.has_value() ? peer_id->toBase58() : "unknown",
                   signed_attestation.message.validator_id);

          auto res = self->fork_choice_store_->onAttestation(signed_attestation,
                                                             false);
          if (not res.has_value()) {
            SL_WARN(self->logger_,
                    "Error processing vote for target={}: {}",
                    signed_attestation.message.data.target,
                    res.error());
            return;
          }
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
      auto slot_hash = message->notification.message.block.slotHash();
      SL_DEBUG(self->logger_,
               "📣 Gossiped block in slot {} hash={:xx} 🔗",
               slot_hash.slot,
               slot_hash.hash);
      self->gossip_blocks_topic_->publish(
          encodeSszSnappy(message->notification));
    });
  }

  void NetworkingImpl::onSendSignedVote(
      std::shared_ptr<const messages::SendSignedVote> message) {
    boost::asio::post(*io_context_, [self{shared_from_this()}, message] {
      SL_DEBUG(self->logger_,
               "📣 Gossiped vote for target={} 🗳️",
               message->notification.message.data.target);
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
          while (auto raw_result = co_await topic->receiveMessage()) {
            auto &raw = raw_result.value();
            if (auto r = decodeSszSnappy<T>(raw.data)) {
              f(std::move(r.value()), raw.received_from);
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
                          block.message.block.setHash();
                          self->receiveBlock(peer_id, std::move(block));
                        }
                      });
  }

  // TODO(turuslan): detect finalized change
  void NetworkingImpl::receiveBlock(
      std::optional<libp2p::PeerId> from_peer,
      SignedBlockWithAttestation &&signed_block_with_attestation) {
    auto slot_hash = signed_block_with_attestation.message.block.slotHash();
    SL_DEBUG(logger_,
             "Received block slot {} hash={:xx} parent={:xx} from peer={}",
             slot_hash.slot,
             slot_hash.hash,
             signed_block_with_attestation.message.block.parent_root,
             from_peer.has_value() ? from_peer->toBase58() : "unknown");

    // Remove function for cached children
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
    auto parent_hash = signed_block_with_attestation.message.block.parent_root;
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
      std::vector<SignedBlockWithAttestation> blocks{
          std::move(signed_block_with_attestation)};

      // Import all cached children
      remove([&](const BlockHash &block_hash) {
        blocks.emplace_back(block_cache_.extract(block_hash).mapped());
      });
      for (auto &block : blocks) {
        auto res = fork_choice_store_->onBlock(block);
        if (not res.has_value()) {
          SL_WARN(logger_,
                  "❌ Error importing block={}: {}",
                  block.message.block.slotHash(),
                  res.error());
          break;
        }
      }
      SL_INFO(logger_,
              "✅ Imported blocks: {}",
              fmt::join(blocks
                            | std::views::transform(
                                [](const SignedBlockWithAttestation &block) {
                                  return block.message.block.slot;
                                }),
                        " "));
      block_tree_->import(std::move(blocks));
      return;
    }
    block_cache_.emplace(slot_hash.hash,
                         std::move(signed_block_with_attestation));
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

  void NetworkingImpl::connectToPeers() {
    auto now = Clock::now();
    auto want = peer_states_.size();
    if (auto &limit = config_->maxBootnodes()) {
      want = std::min(want, *limit);
    }
    size_t active = 0;
    for (auto &state : peer_states_ | std::views::values) {
      if (std::holds_alternative<PeerState::Connecting>(state.state)
          or std::holds_alternative<PeerState::Connected>(state.state)) {
        ++active;
      } else if (auto *backoff =
                     std::get_if<PeerState::Backoff>(&state.state)) {
        if (backoff->backoff_until <= now) {
          // Backoff => Connectable
          state.state = PeerState::Connectable{.backoff = backoff->backoff};
          connectable_peers_.emplace_back(state.info.id);
        }
      }
    }
    if (want <= active) {
      return;
    }
    want -= active;
    while (want != 0 and not connectable_peers_.empty()) {
      --want;
      size_t i = std::uniform_int_distribution<size_t>{
          0, connectable_peers_.size() - 1}(random_);
      std::swap(connectable_peers_.at(i), connectable_peers_.back());
      auto peer_id = std::move(connectable_peers_.back());
      connectable_peers_.pop_back();
      auto &state = peer_states_.at(peer_id);
      auto &connectable = std::get<PeerState::Connectable>(state.state);
      // Connectable => Connecting
      state.state = PeerState::Connecting{.backoff = connectable.backoff};
      libp2p::coroSpawn(
          *io_context_,
          [weak_self{weak_from_this()},
           host{host_},
           peer_info{state.info}]() -> libp2p::Coro<void> {
            auto r = co_await host->connect(peer_info);
            auto self = weak_self.lock();
            if (not self) {
              co_return;
            }
            auto &state = self->peer_states_.at(peer_info.id);
            if (not r.has_value()) {
              SL_WARN(self->logger_,
                      "connect={} error: {}",
                      peer_info.id.toBase58(),
                      r.error());
              if (auto *connecting =
                      std::get_if<PeerState::Connecting>(&state.state)) {
                // Connecting => Backoff
                state.state = PeerState::Backoff{
                    .backoff = std::min(2 * connecting->backoff, kMaxBackoff),
                    .backoff_until = Clock::now() + connecting->backoff,
                };
              }
            }
          });
    }
  }
}  // namespace lean::modules
