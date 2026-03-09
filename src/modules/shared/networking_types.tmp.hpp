/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/peer/peer_id.hpp>

#include "modules/networking/types.hpp"
#include "types/signed_aggregated_attestation.hpp"
#include "types/signed_attestation.hpp"
#include "types/signed_block_with_attestation.hpp"

namespace lean::messages {
  template <typename Notification>
  struct NotificationReceived {
    libp2p::PeerId from_peer;
    Notification notification;
  };

  template <typename Notification>
  struct GossipNotificationReceived {
    Notification notification;
  };

  template <typename Notification>
  struct BroadcastNotification {
    Notification notification;
  };

  struct PeerConnectedMessage {
    libp2p::PeerId peer;
    // address?
    // initial view?
  };

  struct PeerDisconnectedMessage {
    libp2p::PeerId peer;
    // reason?
  };

  struct PeerCountsMessage {
    std::unordered_map<std::string, size_t> map;
  };

  using StatusMessageReceived = NotificationReceived<StatusMessage>;

  using SendSignedBlock = BroadcastNotification<SignedBlockWithAttestation>;

  using SendSignedVote = BroadcastNotification<SignedAttestation>;
  using SignedVoteReceived = GossipNotificationReceived<SignedAttestation>;

  using SendSignedAggregatedAttestation =
      BroadcastNotification<SignedAggregatedAttestation>;
}  // namespace lean::messages
