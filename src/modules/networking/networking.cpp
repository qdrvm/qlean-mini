/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */


#include "modules/networking/networking.hpp"

#include <algorithm>
#include <format>
#include <memory>
#include <queue>
#include <ranges>
#include <stdexcept>

#include <boost/endian/conversion.hpp>
#include <libp2p/coro/spawn.hpp>
#include <libp2p/coro/timer_loop.hpp>
#include <libp2p/crypto/key_marshaller.hpp>
#include <libp2p/crypto/sha/sha256.hpp>
#include <libp2p/host/basic_host.hpp>
#include <libp2p/injector/host_injector.hpp>
#include <libp2p/muxer/muxed_connection_config.hpp>
#include <libp2p/peer/identity_manager.hpp>
#include <libp2p/protocol/gossip/gossip.hpp>
#include <libp2p/protocol/identify.hpp>
#include <libp2p/protocol/ping.hpp>
#include <libp2p/transport/quic/transport.hpp>
#include <libp2p/transport/tcp/tcp_util.hpp>
#include <qtils/to_shared_ptr.hpp>

#include "app/build_version.hpp"
#include "app/chain_spec.hpp"
#include "app/configuration.hpp"
#include "blockchain/block_tree.hpp"
#include "blockchain/fork_choice.hpp"
#include "blockchain/genesis_config.hpp"
#include "blockchain/validator_registry.hpp"
#include "blockchain/validator_subnet.hpp"
#include "metrics/metrics.hpp"
#include "modules/networking/block_request_protocol.hpp"
#include "modules/networking/ssz_snappy.hpp"
#include "modules/networking/status_protocol.hpp"
#include "modules/networking/types.hpp"
#include "ssl_context.hpp"
#include "state_sync_client.hpp"

namespace lean::modules {
  constexpr std::chrono::seconds kConnectToPeersTimer{5};
  constexpr std::chrono::milliseconds kInitBackoff = std::chrono::seconds{10};
  constexpr std::chrono::milliseconds kMaxBackoff = std::chrono::minutes{5};

  constexpr auto kRetryRequestBlock = std::chrono::seconds{3};

  template <typename T>
  std::vector<typename T::mapped_type> consumeMultimap(
      T &multimap, const typename T::key_type &key) {
    auto [begin, end] = multimap.equal_range(key);
    std::vector<typename T::mapped_type> vector;
    for (auto &value : std::ranges::subrange{begin, end} | std::views::values) {
      vector.emplace_back(value);
    }
    multimap.erase(begin, end);
    return vector;
  }

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
    if (auto uncompressed_res = snappy::uncompress(message.data)) {
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
      qtils::SharedRef<metrics::Metrics> metrics,
      qtils::SharedRef<app::StateManager> app_state_manager,
      qtils::SharedRef<blockchain::BlockTree> block_tree,
      qtils::SharedRef<lean::ForkChoiceStore> fork_choice_store,
      qtils::SharedRef<ValidatorRegistry> validator_registry,
      qtils::SharedRef<GenesisConfig> genesis_config,
      qtils::SharedRef<app::ChainSpec> chain_spec,
      qtils::SharedRef<app::Configuration> config)
      : loader_(loader),
        logger_(logging_system->getLogger("Networking", "networking_module")),
        metrics_{std::move(metrics)},
        app_state_manager_{std::move(app_state_manager)},
        block_tree_{std::move(block_tree)},
        fork_choice_store_{std::move(fork_choice_store)},
        validator_registry_{std::move(validator_registry)},
        genesis_config_{std::move(genesis_config)},
        chain_spec_{std::move(chain_spec)},
        config_{std::move(config)},
        random_{std::random_device{}()},
        subnet_count_{config_->cliSubnetCount()} {
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

    auto keypair = config_->nodeKey();

    // Always set up identity and peer info
    libp2p::peer::IdentityManager identity_manager{
        keypair,
        std::make_shared<libp2p::crypto::marshaller::KeyMarshaller>(nullptr)};
    auto peer_id = identity_manager.getId();

    std::unordered_set<SubnetIndex> subnets;
    for (auto &validator_index :
         validator_registry_->currentValidatorIndices()) {
      subnets.emplace(validatorSubnet(validator_index, subnet_count_));
    }
    if (subnets.size() != 1) {
      SL_FATAL(logger_, "multiple validators on same node are not supported");
    }
    auto subnet_id = *subnets.begin();

    metrics_->lean_attestation_committee_subnet()->set(subnet_id);
    metrics_->lean_attestation_committee_count()->set(subnet_count_);

    SL_INFO(logger_, "Networking loaded with PeerId {}", peer_id.toBase58());

    libp2p::protocol::gossip::Config gossip_config;
    gossip_config.validation_mode =
        libp2p::protocol::gossip::ValidationMode::Anonymous;
    gossip_config.message_authenticity =
        libp2p::protocol::gossip::MessageAuthenticity::Anonymous;
    gossip_config
        .message_id_fn = [logger = logger_](
                             const libp2p::protocol::gossip::Message &message) {
      std::string_view topic(
          reinterpret_cast<const char *>(message.topic.data()),
          message.topic.size());
      SL_TRACE(
          logger,
          "GossipMessageID: 📨 Received message topic={} size(compressed)={}",
          topic,
          message.data.size());
      return gossipMessageId(message);
    };
    gossip_config.protocol_versions.erase(
        libp2p::protocol::gossip::kProtocolGossipsubv1_2);

    // Use a shorter no-streams interval to close idle connections faster.
    // This helps nodes rejoin the gossip mesh more quickly after a restart,
    // especially if they reconnect before the old connection on the peer's side
    // has timed out.
    libp2p::muxer::MuxedConnectionConfig mux_config;
    mux_config.no_streams_interval = std::chrono::seconds{10};

    auto identify_config = std::make_shared<libp2p::protocol::IdentifyConfig>();
    identify_config->agent_version = "qlean/" + buildVersion();

    auto injector = qtils::toSharedPtr(libp2p::injector::makeHostInjector(
        libp2p::injector::useKeyPair(keypair),
        libp2p::injector::useGossipConfig(std::move(gossip_config)),
        boost::di::bind<libp2p::muxer::MuxedConnectionConfig>().to(
            mux_config)[boost::di::override],
        libp2p::injector::useTransportAdaptors<
            libp2p::transport::QuicTransport>(),
        boost::di::bind<libp2p::protocol::IdentifyConfig>().to(
            identify_config)[boost::di::override]));
    injector_ = injector;
    io_context_ = injector->create<std::shared_ptr<boost::asio::io_context>>();

    auto host = injector->create<std::shared_ptr<libp2p::host::BasicHost>>();
    host_ = host;

    bool has_enr_listen_address = false;
    const auto &bootnodes = chain_spec_->getBootnodes();
    for (auto &index : validator_registry_->allValidatorsIndices()) {
      auto name = validator_registry_->nodeIdByIndex(index).value();
      if (index >= bootnodes.getBootnodes().size()) {
        SL_WARN(logger_, "No ENR for validator {}", index);
        continue;
      }
      auto &peer_id = bootnodes.getBootnodes().at(index).peer_id;
      peer_name_.emplace(peer_id, name);
    }
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
      connectable_peers_.reserve(bootnodes.size());
      for (auto &&[validator_index, bootnode] : std::views::zip(
               std::views::iota(
                   ValidatorIndex{0},
                   ValidatorIndex{genesis_config_->validator_count}),
               bootnodes.getBootnodes())) {
        if (bootnode.peer_id == peer_id) {
          continue;
        }
        if (bootnode.is_aggregator
            and subnets.contains(
                validatorSubnet(validator_index, subnet_count_))) {
          subnet_aggregators_.emplace(bootnode.peer_id);
        } else {
          connectable_peers_.emplace_back(bootnode.peer_id);
        }
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
                   "Added bootnode, address={}",
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

      const auto direction_label =
          connection->isInitiator() ? "inbound" : "outbound";

      // TODO Implement result extraction: "success", "timeout", "error"
      //  Issue: https://github.com/qdrvm/qlean-mini/issues/60#sync-metrics
      const auto result_label = "success";

      self->metrics_
          ->network_connect_event_count(
              {{"direction", direction_label}, {"result", result_label}})
          ->inc();

      SL_TRACE(self->logger_,
               "🔗 Peer connected: {}",
               connection->remotePeer().toBase58());
      auto peer_id = connection->remotePeer();
      auto state_it = self->peer_states_.find(peer_id);
      if (state_it != self->peer_states_.end()) {
        auto &state = state_it->second;
        if (not std::holds_alternative<PeerState::Connected>(state.state)) {
          if (std::holds_alternative<PeerState::Connectable>(state.state)) {
            // Connectable => Connected
            SL_DEBUG(self->logger_,
                     "Peer {} state transition: Connectable -> Connected",
                     peer_id.toBase58());
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
          SL_INFO(
              self->logger_, "Peer {} marked Connected", peer_id.toBase58());
        }
      }
      self->updateMetricConnectedPeerCount();
      self->loader_.dispatch_peer_connected(
          qtils::toSharedPtr(messages::PeerConnectedMessage{peer_id}));
      if (connection->isInitiator()) {
        SL_DEBUG(self->logger_,
                 "Peer {} is initiator — starting status handshake",
                 peer_id.toBase58());
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
        SL_DEBUG(self->logger_,
                 "Non-initiator peer {} remote address={}",
                 peer_id.toBase58(),
                 addr_res.value().getStringAddress());

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
        } else {
          SL_DEBUG(self->logger_,
                   "Successfully added remote address for peer {}",
                   peer_id.toBase58());
        }
      }
    };

    auto on_peer_disconnected = [weak_self{weak_from_this()}](
                                    libp2p::PeerId peer_id) {
      auto self = weak_self.lock();
      if (not self) {
        return;
      }
      SL_TRACE(self->logger_, "🔌 Peer disconnected: {}", peer_id.toBase58());
      auto state_it = self->peer_states_.find(peer_id);
      if (state_it != self->peer_states_.end()) {
        auto &state = state_it->second;
        if (std::holds_alternative<PeerState::Connected>(state.state)) {
          auto backoff = kInitBackoff;
          SL_DEBUG(
              self->logger_,
              "Peer {} state transition: Connected -> Backoff (backoff={}ms)",
              peer_id.toBase58(),
              std::chrono::duration_cast<std::chrono::milliseconds>(backoff)
                  .count());
          // Connected => Backoff
          state.state = PeerState::Backoff{
              .backoff = backoff,
              .backoff_until = Clock::now() + backoff,
          };
          SL_DEBUG(
              self->logger_, "Peer {} backoff_until set", peer_id.toBase58());
        }
      } else {
        SL_DEBUG(self->logger_,
                 "on_peer_disconnected: unknown peer {}",
                 peer_id.toBase58());
      }
      self->host_->getPeerRepository().getUserAgentRepository().updateTtl(
          peer_id, libp2p::peer::ttl::kTransient);
      self->updateMetricConnectedPeerCount();
      self->loader_.dispatch_peer_disconnected(
          qtils::toSharedPtr(messages::PeerDisconnectedMessage{peer_id}));
      SL_TRACE(self->logger_,
               "Dispatched PeerDisconnectedMessage for {}",
               peer_id.toBase58());
    };

    auto on_connection_closed =
        [weak_self{weak_from_this()}](
            std::shared_ptr<libp2p::connection::CapableConnection> connection) {
          auto self = weak_self.lock();
          if (not self) {
            return;
          }

          const auto direction_label =
              connection->isInitiator() ? "inbound" : "outbound";

          // TODO Implement reason extraction:
          //  "timeout", "remote_close", "local_close", "error"
          //  Issue: https://github.com/qdrvm/qlean-mini/issues/60#sync-metrics
          const auto reason_label = "unknown";

          self->metrics_
              ->network_disconnect_event_count(
                  {{"direction", direction_label}, {"reason", reason_label}})
              ->inc();

          SL_TRACE(self->logger_,
                   "❌ Connection with {} ({}) closed; reason: {}",
                   connection->remotePeer().toBase58(),
                   direction_label,
                   reason_label);
        };

    on_peer_connected_sub_ =
        host->getBus()
            .getChannel<libp2p::event::network::OnNewConnectionChannel>()
            .subscribe(on_peer_connected);
    on_peer_disconnected_sub_ =
        host->getBus()
            .getChannel<libp2p::event::network::OnPeerDisconnectedChannel>()
            .subscribe(on_peer_disconnected);
    on_connection_closed_sub_ =
        host->getBus()
            .getChannel<libp2p::event::network::OnConnectionClosedChannel>()
            .subscribe(on_connection_closed);

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
        std::format("attestation_{}", subnet_id),
        [weak_self{weak_from_this()}](SignedAttestation &&signed_attestation,
                                      std::optional<libp2p::PeerId> peer_id) {
          auto self = weak_self.lock();
          if (not self) {
            return;
          }

          SL_DEBUG(self->logger_,
                   "Received vote for target={} 🗳️ from peer={} 👤 "
                   "validator_id={} ✅",
                   signed_attestation.message.target,
                   peer_id.has_value() ? peer_id->toBase58() : "unknown",
                   signed_attestation.validator_id);

          auto &head = signed_attestation.message.head;
          if (not self->block_tree_->has(head.root)) {
            if (head.slot <= self->block_tree_->lastFinalized().slot) {
              SL_WARN(self->logger_, "Pending attestation for finalized fork");
              return;
            }
            SL_INFO(self->logger_,
                    "Pending attestation from validator {} for head {}",
                    signed_attestation.validator_id,
                    head);
            self->attestation_cache_.emplace(head.root, signed_attestation);
            if (peer_id.has_value()) {
              self->requestBlock(*peer_id, head.root);
            }
            return;
          }
          auto res =
              self->fork_choice_store_->onGossipAttestation(signed_attestation);
          if (not res.has_value()) {
            SL_WARN(self->logger_,
                    "Error processing vote for target={}: {}",
                    signed_attestation.message.target,
                    res.error());
            return;
          }
        });
    gossip_signed_aggregated_attestation_topic_ =
        gossipSubscribe<SignedAggregatedAttestation>(
            "aggregation",
            [weak_self{weak_from_this()}](
                SignedAggregatedAttestation &&signed_aggregated_attestation,
                std::optional<libp2p::PeerId> peer_id) {
              auto self = weak_self.lock();
              if (not self) {
                return;
              }

              SL_DEBUG(
                  self->logger_,
                  "Received aggregated attestation for target={} 🗳️ from "
                  "peer={} 👤 "
                  "validator_ids=[{}] ✅",
                  signed_aggregated_attestation.data.target,
                  peer_id.has_value() ? peer_id->toBase58() : "unknown",
                  fmt::join(
                      signed_aggregated_attestation.proof.participants.iter(),
                      " "));

              auto &head = signed_aggregated_attestation.data.head;
              if (not self->block_tree_->has(head.root)) {
                if (head.slot <= self->block_tree_->lastFinalized().slot) {
                  SL_WARN(self->logger_,
                          "Pending aggregated attestation for finalized fork");
                  return;
                }
                SL_INFO(
                    self->logger_,
                    "Pending attestation from validators [{}] for head {}",
                    fmt::join(
                        signed_aggregated_attestation.proof.participants.iter(),
                        " "),
                    head);
                self->aggregated_attestation_cache_.emplace(
                    head.root, signed_aggregated_attestation);
                if (peer_id.has_value()) {
                  self->requestBlock(*peer_id, head.root);
                }
                return;
              }
              auto res =
                  self->fork_choice_store_->onGossipAggregatedAttestation(
                      signed_aggregated_attestation);
              if (not res.has_value()) {
                SL_WARN(
                    self->logger_,
                    "Error processing aggregated attestation for target={}: {}",
                    signed_aggregated_attestation.data.target,
                    res.error());
                return;
              }
            });

    io_thread_.emplace([io_context{io_context_}] {
      auto work_guard = boost::asio::make_work_guard(*io_context);
      io_context->run();
    });

    ssl_context_ = std::make_shared<AsioSslContext>();
    state_sync_client_ =
        std::make_unique<StateSyncClient>(ssl_context_, app_state_manager_);
  }

  void NetworkingImpl::on_loading_is_finished() {
    SL_INFO(logger_, "Loading is finished");
  }

  void NetworkingImpl::onSendSignedBlock(
      std::shared_ptr<const messages::SendSignedBlock> message) {
    boost::asio::post(*io_context_, [self{shared_from_this()}, message] {
      auto slot_hash = message->notification.message.block.index();
      SL_DEBUG(self->logger_,
               "📣 Gossiped block in slot {} hash={:0xx} 🔗",
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
               message->notification.message.target);
      self->gossip_votes_topic_->publish(
          encodeSszSnappy(message->notification));
    });
  }

  void NetworkingImpl::onSendSignedAggregatedAttestation(
      std::shared_ptr<const messages::SendSignedAggregatedAttestation>
          message) {
    boost::asio::post(*io_context_, [self{shared_from_this()}, message] {
      SL_DEBUG(self->logger_,
               "📣 Gossiped aggregated attestation for target={} 🗳️",
               message->notification.data.target);
      self->gossip_signed_aggregated_attestation_topic_->publish(
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
            } else {
              SL_WARN(this->logger_,
                      "❌ Error decoding Gossip message for type {}: {}",
                      type,
                      r.error().message());
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
    if (head.slot > block_tree_->lastFinalized().slot
        and not block_tree_->has(head.hash)) {
      SL_TRACE(logger_, "receiveStatus {} => request", head.slot);
      requestBlock(message.from_peer, head.hash);
    }
    loader_.dispatchStatusMessageReceived(qtils::toSharedPtr(message));
  }

  void NetworkingImpl::requestBlock(const libp2p::PeerId &peer_id,
                                    BlockHash block_hash) {
    // find missing block or parent
    auto finalized = block_tree_->lastFinalized();
    while (true) {
      auto block_it = block_cache_.find(block_hash);
      if (block_it == block_cache_.end()) {
        break;
      }
      auto &block = block_it->second.block.message.block;
      // ignore finalized fork
      if (block.slot <= finalized.slot) {
        return;
      }
      block_hash = block.parent_root;
    }
    // ignore existing block
    if (block_tree_->has(block_hash)) {
      return;
    }

    // wait before retry
    auto now = Clock::now();
    auto &requested_at = block_requested_at_[block_hash];
    if (now < requested_at + kRetryRequestBlock) {
      return;
    }
    requested_at = now;

    auto name_it = peer_name_.find(peer_id);
    auto peer_name =
        name_it != peer_name_.end() ? name_it->second : peer_id.toBase58();
    SL_INFO(logger_, "request block {} from {}", block_hash, peer_name);

    libp2p::coroSpawn(
        *io_context_,
        [self{shared_from_this()}, peer_id, block_hash, peer_name]()
            -> libp2p::Coro<void> {
          auto response_res = co_await self->block_request_protocol_->request(
              peer_id, {.blocks = {{block_hash}}});
          self->block_requested_at_.erase(block_hash);
          if (response_res.has_value()) {
            auto &block = response_res.value();
            SL_DEBUG(self->logger_,
                     "request block {} from {} success, slot {}",
                     block_hash,
                     peer_name,
                     block.message.block.slot);
            block.message.block.setHash();
            self->receiveBlock(peer_id, std::move(block));
          } else {
            SL_WARN(self->logger_,
                    "request block {} from {} error: {}",
                    block_hash,
                    peer_name,
                    response_res.error());
          }
        });
  }

  void NetworkingImpl::receiveBlock(
      std::optional<libp2p::PeerId> from_peer,
      SignedBlockWithAttestation &&signed_block_with_attestation) {
    auto block_index = signed_block_with_attestation.message.block.index();
    auto &parent_hash = signed_block_with_attestation.message.block.parent_root;

    SL_DEBUG(logger_,
             "Received block {} parent={:0xx} from peer={}",
             block_index,
             signed_block_with_attestation.message.block.parent_root,
             from_peer.has_value() ? from_peer->toBase58() : "unknown");

    // Ignore cached block
    if (block_cache_.contains(block_index.hash)) {
      SL_TRACE(logger_,
               "receiveBlock {} => Block was ignored as cached",
               block_index.slot);
      if (from_peer) {
        requestBlock(*from_peer, parent_hash);
      }
      return;
    }

    // Ignore block imported earlier
    if (block_tree_->has(block_index.hash)) {
      SL_TRACE(logger_,
               "receiveBlock {} => Block was ignored as imported earlier",
               block_index.slot);
      return;
    }

    // Ignore blocks of finalized forks
    if (block_index.slot <= block_tree_->lastFinalized().slot) {
      SL_TRACE(
          logger_,
          "receiveBlock {} => Block was ignored as block of finalised slot",
          block_index.slot);

      // Forget cached children
      auto [child_begin, child_end] =
          block_children_.equal_range(block_index.hash);
      for (auto &child :
           std::ranges::subrange{child_begin, child_end} | std::views::values) {
        consumeBlockTree(false, child, nullptr);
      }
      return;
    }

    auto child_it = block_children_.emplace(parent_hash, block_index.hash);
    block_cache_.emplace(block_index.hash,
                         BlockCacheItem{
                             .child_it = child_it,
                             .block = std::move(signed_block_with_attestation),
                         });

    if (from_peer) {
      requestBlock(*from_peer, parent_hash);
    }

    // If the parent isn't in the tree-cache block and request of parent
    if (not block_tree_->has(parent_hash)) {
      return;
    }

    auto consume = [&](bool good,
                       SignedBlockWithAttestation block,
                       std::vector<SignedAttestation> attestations,
                       std::vector<SignedAggregatedAttestation>
                           aggregated_attestations) {
      auto block_res = fork_choice_store_->onBlock(block);
      if (not block_res.has_value()) {
        SL_WARN(logger_,
                "❌ Error importing block={}: {}",
                block.message.block.index(),
                block_res.error());
        return false;
      }
      SL_INFO(logger_, "✅ Imported block {}", block.message.block.index());

      for (auto &attestation : attestations) {
        SL_INFO(logger_,
                "Import pending attestation from validator {}",
                attestation.validator_id);
        auto res = fork_choice_store_->onGossipAttestation(attestation);
        if (not res.has_value()) {
          SL_WARN(logger_,
                  "Error importing pending attestation from validator {}: {}",
                  attestation.validator_id,
                  res.error());
        }
      }

      for (auto &attestation : aggregated_attestations) {
        SL_INFO(logger_,
                "Import pending attestation from validators [{}]",
                fmt::join(attestation.proof.participants.iter(), " "));
        auto res =
            fork_choice_store_->onGossipAggregatedAttestation(attestation);
        if (not res.has_value()) {
          SL_WARN(
              logger_,
              "Error importing pending attestation from validators [{}]: {}",
              fmt::join(attestation.proof.participants.iter(), " "),
              res.error());
        }
      }

      return true;
    };
    consumeBlockTree(true, block_index.hash, consume);

    // Cleanup cache from blocks of finalized forks
    prune();
  }

  bool NetworkingImpl::statusFinalizedIsGood(const BlockIndex &slot_hash) {
    if (auto expected = block_tree_->getSlotByHash(slot_hash.hash)) {
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
    SL_TRACE(logger_, "connectToPeers: computed want={}", want);
    size_t active = 0;
    for (auto &state : peer_states_ | std::views::values) {
      if (std::holds_alternative<PeerState::Connecting>(state.state)
          or std::holds_alternative<PeerState::Connected>(state.state)) {
        ++active;
      } else if (auto *backoff =
                     std::get_if<PeerState::Backoff>(&state.state)) {
        if (backoff->backoff_until <= now) {
          // Backoff => Connectable
          SL_DEBUG(logger_,
                   "Peer {} backoff expired (backoff={}ms) => Connectable",
                   state.info.id.toBase58(),
                   std::chrono::duration_cast<std::chrono::milliseconds>(
                       backoff->backoff)
                       .count());
          state.state = PeerState::Connectable{.backoff = backoff->backoff};
          if (not subnet_aggregators_.contains(state.info.id)) {
            connectable_peers_.emplace_back(state.info.id);
          }
        }
      }
    }
    SL_TRACE(logger_,
             "connectToPeers: active={}, connectable_candidates={}",
             active,
             connectable_peers_.size());
    auto connect = [&](PeerState &state) {
      ++active;
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
            SL_TRACE(self->logger_,
                     "connectToPeers: connection attempt finished for peer {}",
                     peer_info.id.toBase58());
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
                SL_DEBUG(self->logger_,
                         "Peer {} moved to Backoff (new backoff={}ms)",
                         peer_info.id.toBase58(),
                         std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::min(2 * connecting->backoff, kMaxBackoff))
                             .count());
              }
            } else {
              SL_INFO(self->logger_,
                      "Successfully connected to peer {}",
                      peer_info.id.toBase58());
            }
          });
    };
    for (auto &peer_id : subnet_aggregators_) {
      auto &state = peer_states_.at(peer_id);
      if (not std::holds_alternative<PeerState::Connectable>(state.state)) {
        continue;
      }
      connect(state);
    }
    if (want <= active) {
      SL_TRACE(logger_,
               "connectToPeers: want ({}) <= active ({}), returning",
               want,
               active);
    }
    while (active < want and not connectable_peers_.empty()) {
      size_t i = std::uniform_int_distribution<size_t>{
          0, connectable_peers_.size() - 1}(random_);
      std::swap(connectable_peers_.at(i), connectable_peers_.back());
      auto peer_id = std::move(connectable_peers_.back());
      connectable_peers_.pop_back();
      SL_DEBUG(
          logger_,
          "connectToPeers: initiating connect to peer {} (selected index={})",
          peer_id.toBase58(),
          i);
      auto &state = peer_states_.at(peer_id);
      connect(state);
    }
  }

  void NetworkingImpl::updateMetricConnectedPeerCount() {
    // currently metrics don't forget labels, explicitly reset their count
    for (auto &count : connected_peer_count_by_name_ | std::views::values) {
      count = 0;
    }

    const auto &ua_repo = host_->getPeerRepository().getUserAgentRepository();

    for (const auto &peer_id : host_->getConnectedPeers()) {
      auto name_it = peer_name_.find(peer_id);
      if (name_it != peer_name_.end()) {
        ++connected_peer_count_by_name_[name_it->second];
        continue;
      }

      auto ua = ua_repo.getUserAgent(peer_id).value_or("");
      // To lower case + drop version if any
      for (size_t i = 0; i < ua.size(); ++i) {
        auto &ch = ua[i];
        if (ch == '/') {
          ua.resize(i);
          break;
        }
        if (ch >= 'A' and ch <= 'Z') {
          ch = static_cast<char>(ch - 'A' + 'a');
        }
      }
      if (ua.empty()) {
        ua = "unknown";
      }
      ++connected_peer_count_by_name_[ua];
    }

    for (const auto &[kind, number] : connected_peer_count_by_name_) {
      metrics_->network_connected_peer_count({{"client", kind}})->set(number);
    }

    loader_.dispatch_peers_total_count_updated(
        std::make_shared<messages::PeerCountsMessage>(
            connected_peer_count_by_name_));
  }

  void NetworkingImpl::prune() {
    auto finalized = block_tree_->lastFinalized();
    std::erase_if(block_cache_,
                  [&](const decltype(block_cache_)::value_type &p) {
                    if (p.second.block.message.block.slot <= finalized.slot) {
                      block_children_.erase(p.second.child_it);
                      return true;
                    }
                    return false;
                  });
    std::erase_if(attestation_cache_,
                  [&](const decltype(attestation_cache_)::value_type &p) {
                    return p.second.message.head.slot <= finalized.slot;
                  });
    std::erase_if(
        aggregated_attestation_cache_,
        [&](const decltype(aggregated_attestation_cache_)::value_type &p) {
          return p.second.data.head.slot <= finalized.slot;
        });
  }

  void NetworkingImpl::consumeBlockTree(bool init_good,
                                        BlockHash init_hash,
                                        BlockConsumer consume) {
    std::deque queue{std::make_pair(init_good, init_hash)};
    while (not queue.empty()) {
      const auto [good, hash] = queue.front();
      queue.pop_front();
      auto block_it = block_cache_.find(hash);
      if (block_it == block_cache_.end()) {
        SL_FATAL(logger_, "consumeBlockTree inconsistent block {}", hash);
      }
      auto block = std::move(block_it->second.block);
      block_children_.erase(block_it->second.child_it);
      block_cache_.erase(block_it);
      auto attestations = consumeMultimap(attestation_cache_, hash);
      auto aggregated_attestations =
          consumeMultimap(aggregated_attestation_cache_, hash);
      const auto new_good = consume
                              ? consume(good,
                                        std::move(block),
                                        std::move(attestations),
                                        std::move(aggregated_attestations))
                              : good;
      auto [child_begin, child_end] = block_children_.equal_range(hash);
      for (auto &child :
           std::ranges::subrange{child_begin, child_end} | std::views::values) {
        queue.emplace_back(new_good, child);
      }
    }
  }
}  // namespace lean::modules
