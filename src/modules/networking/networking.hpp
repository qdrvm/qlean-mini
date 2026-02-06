/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <random>
#include <thread>

#include <libp2p/event/bus.hpp>
#include <libp2p/peer/peer_info.hpp>
#include <log/logger.hpp>
#include <modules/networking/interfaces.hpp>
#include <qtils/create_smart_pointer_macros.hpp>
#include <qtils/shared_ref.hpp>
#include <utils/ctor_limiters.hpp>

namespace boost::asio {
  class io_context;
}  // namespace boost::asio

namespace libp2p::host {
  class BasicHost;
}  // namespace libp2p::host

namespace libp2p::protocol {
  class Ping;
  class Identify;
}  // namespace libp2p::protocol

namespace libp2p::protocol::gossip {
  class Gossip;
  class Topic;
}  // namespace libp2p::protocol::gossip

namespace lean {
  struct AsioSslContext;
  class StateSyncClient;
  class ForkChoiceStore;
}  // namespace lean

namespace lean::app {
  class StateManager;
  class ChainSpec;
  class Configuration;
}  // namespace lean::app

namespace lean::blockchain {
  class BlockTree;
}  // namespace lean::blockchain

namespace lean::metrics {
  class Metrics;
}  // namespace lean::metrics

namespace lean::modules {
  class StatusProtocol;
  class BlockRequestProtocol;

  using Clock = std::chrono::steady_clock;

  /**
   * Peer information and state.
   * Peer can be in connectable list, be connecting, be connected, be backedoff.
   */
  struct PeerState {
    /**
     * Peer not connected.
     */
    struct Connectable {
      /**
       * Backoff to use on next connect failure.
       */
      std::chrono::milliseconds backoff;
    };
    /**
     * Connecting to peer.
     */
    struct Connecting {
      /**
       * Backoff to use on next connect failure.
       */
      std::chrono::milliseconds backoff;
    };
    /**
     * Connected to peer.
     */
    struct Connected {};
    /**
     * Don't connect
     */
    struct Backoff {
      /**
       * Backoff to use on next connect failure.
       */
      std::chrono::milliseconds backoff;
      /**
       * Won't attempt to connect until this time.
       */
      Clock::time_point backoff_until;
    };
    /**
     * Peer id and addresses to connect to.
     */
    libp2p::PeerInfo info;
    std::variant<Connectable, Connecting, Connected, Backoff> state;
  };

  /**
   * Network module.
   *
   * Sends produced blocks and signed votes.
   * Syncs blocks from other peers.
   * Receives votes from other peers.
   *
   * Protocols:
   * - Status handshake protocol (best and finalized block info).
   * - Block request protocol (`SignedBlockWithAttestation` by hash).
   * - `SignedBlockWithAttestation` and `SignedAttestation` gossip protocol.
   */
  class NetworkingImpl final : public Singleton<Networking>, public Networking {
    NetworkingImpl(NetworkingLoader &loader,
                   qtils::SharedRef<log::LoggingSystem> logging_system,
                   qtils::SharedRef<metrics::Metrics> metrics,
                   qtils::SharedRef<app::StateManager> app_state_manager,
                   qtils::SharedRef<blockchain::BlockTree> block_tree,
                   qtils::SharedRef<ForkChoiceStore> fork_choice_store,
                   qtils::SharedRef<app::ChainSpec> chain_spec,
                   qtils::SharedRef<app::Configuration> config);

   public:
    CREATE_SHARED_METHOD(NetworkingImpl);

    ~NetworkingImpl() override;

    // Networking
    void on_loaded_success() override;
    void on_loading_is_finished() override;
    void onSendSignedBlock(
        std::shared_ptr<const messages::SendSignedBlock> message) override;
    void onSendSignedVote(
        std::shared_ptr<const messages::SendSignedVote> message) override;

   private:
    template <typename T>
    std::shared_ptr<libp2p::protocol::gossip::Topic> gossipSubscribe(
        std::string_view type, auto f);

    void receiveStatus(const messages::StatusMessageReceived &message);
    void requestBlock(const libp2p::PeerId &peer_id,
                      const BlockHash &block_hash);
    void receiveBlock(std::optional<libp2p::PeerId> peer_id,
                      SignedBlockWithAttestation &&block);
    bool statusFinalizedIsGood(const BlockIndex &slot_hash);
    /**
     * Called periodically to connect to more peers if there are not enough
     * connections.
     */
    void connectToPeers();
    void updateMetricConnectedPeerCount();

    NetworkingLoader &loader_;
    log::Logger logger_;
    qtils::SharedRef<metrics::Metrics> metrics_;
    qtils::SharedRef<app::StateManager> app_state_manager_;
    qtils::SharedRef<blockchain::BlockTree> block_tree_;
    qtils::SharedRef<ForkChoiceStore> fork_choice_store_;
    qtils::SharedRef<app::ChainSpec> chain_spec_;
    qtils::SharedRef<app::Configuration> config_;
    std::shared_ptr<void> injector_;
    std::shared_ptr<boost::asio::io_context> io_context_;
    std::optional<std::thread> io_thread_;
    libp2p::event::Handle on_peer_connected_sub_;
    libp2p::event::Handle on_peer_disconnected_sub_;
    libp2p::event::Handle on_connection_closed_sub_;
    std::shared_ptr<StatusProtocol> status_protocol_;
    std::shared_ptr<BlockRequestProtocol> block_request_protocol_;
    std::shared_ptr<libp2p::protocol::gossip::Gossip> gossip_;
    std::shared_ptr<libp2p::protocol::Ping> ping_;
    std::shared_ptr<libp2p::host::BasicHost> host_;
    std::shared_ptr<libp2p::protocol::Identify> identify_;
    std::shared_ptr<libp2p::protocol::gossip::Topic> gossip_blocks_topic_;
    std::shared_ptr<libp2p::protocol::gossip::Topic> gossip_votes_topic_;
    std::unordered_map<BlockHash, SignedBlockWithAttestation> block_cache_;
    std::unordered_multimap<BlockHash, BlockHash> block_children_;
    std::default_random_engine random_;
    std::shared_ptr<AsioSslContext> ssl_context_;
    std::unique_ptr<StateSyncClient> state_sync_client_;
    /**
     * Array of connectable peers to pick random peer from.
     */
    std::vector<libp2p::PeerId> connectable_peers_;
    /**
     * Bootnode peers states.
     */
    std::unordered_map<libp2p::PeerId, PeerState> peer_states_;
  };

}  // namespace lean::modules
