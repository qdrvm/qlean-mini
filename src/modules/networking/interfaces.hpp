/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <modules/shared/networking_types.tmp.hpp>

namespace lean::modules {

  struct NetworkingLoader {
    virtual ~NetworkingLoader() = default;

    virtual void dispatch_peer_connected(
        std::shared_ptr<const messages::PeerConnectedMessage> msg) = 0;

    virtual void dispatch_peer_disconnected(
        std::shared_ptr<const messages::PeerDisconnectedMessage> msg) = 0;

    virtual void dispatch_peers_total_count_updated(
        std::shared_ptr<const messages::PeersTotalCountMessage> msg) = 0;

    virtual void dispatchStatusMessageReceived(
        std::shared_ptr<const messages::StatusMessageReceived> message) = 0;
    virtual void dispatchSignedVoteReceived(
        std::shared_ptr<const messages::SignedVoteReceived> message) = 0;
  };

  struct Networking {
    virtual ~Networking() = default;

    virtual void on_loaded_success() = 0;

    virtual void on_loading_is_finished() = 0;

    virtual void onSendSignedBlock(
        std::shared_ptr<const messages::SendSignedBlock> message) = 0;
    virtual void onSendSignedVote(
        std::shared_ptr<const messages::SendSignedVote> message) = 0;
    virtual void onSendSignedAggregatedAttestation(
        std::shared_ptr<const messages::SendSignedAggregatedAttestation>
            message) = 0;
  };

}  // namespace lean::modules
