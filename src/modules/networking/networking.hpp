/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <thread>

#include <libp2p/event/bus.hpp>
#include <log/logger.hpp>
#include <modules/networking/interfaces.hpp>
#include <qtils/create_smart_pointer_macros.hpp>
#include <qtils/shared_ref.hpp>
#include <utils/ctor_limiters.hpp>

namespace boost::asio {
  class io_context;
}  // namespace boost::asio

namespace libp2p::protocol {
  class Ping;
  class Identify;
}  // namespace libp2p::protocol

namespace libp2p::protocol::gossip {
  class Gossip;
  class Topic;
}  // namespace libp2p::protocol::gossip

namespace lean {
  class ForkChoiceStore;
}  // namespace lean

namespace lean::blockchain {
  class BlockTree;
}  // namespace lean::blockchain

namespace lean::app {
  class ChainSpec;
  class Configuration;
}  // namespace lean::app

namespace lean::modules {
  class StatusProtocol;
  class BlockRequestProtocol;

  /**
   * Network module.
   *
   * Sends produced blocks and signed votes.
   * Syncs blocks from other peers.
   * Receives votes from other peers.
   *
   * Protocols:
   * - Status handshake protocol (best and finalized block info).
   * - Block request protocol (`SignedBlock` by hash).
   * - `SignedBlock` and `SignedVote` gossip protocol.
   */
  class NetworkingImpl final : public Singleton<Networking>, public Networking {
    NetworkingImpl(NetworkingLoader &loader,
                   qtils::SharedRef<log::LoggingSystem> logging_system,
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
                      SignedBlock &&block);
    bool statusFinalizedIsGood(const BlockIndex &slot_hash);

    NetworkingLoader &loader_;
    log::Logger logger_;
    qtils::SharedRef<blockchain::BlockTree> block_tree_;
    qtils::SharedRef<ForkChoiceStore> fork_choice_store_;
    qtils::SharedRef<app::ChainSpec> chain_spec_;
    qtils::SharedRef<app::Configuration> config_;
    std::shared_ptr<void> injector_;
    std::shared_ptr<boost::asio::io_context> io_context_;
    std::optional<std::thread> io_thread_;
    libp2p::event::Handle on_peer_connected_sub_;
    libp2p::event::Handle on_peer_disconnected_sub_;
    std::shared_ptr<StatusProtocol> status_protocol_;
    std::shared_ptr<BlockRequestProtocol> block_request_protocol_;
    std::shared_ptr<libp2p::protocol::gossip::Gossip> gossip_;
    std::shared_ptr<libp2p::protocol::Ping> ping_;
    std::shared_ptr<libp2p::protocol::Identify> identify_;
    std::shared_ptr<libp2p::protocol::gossip::Topic> gossip_blocks_topic_;
    std::shared_ptr<libp2p::protocol::gossip::Topic> gossip_votes_topic_;
    std::unordered_map<BlockHash, SignedBlock> block_cache_;
    std::unordered_multimap<BlockHash, BlockHash> block_children_;
  };

}  // namespace lean::modules
