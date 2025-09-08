/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/peer/peer_id.hpp>

#include "types/signed_block.hpp"
#include "types/signed_vote.hpp"
#include "types/status_message.hpp"

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

  using StatusMessageReceived = NotificationReceived<StatusMessage>;

  using SendSignedBlock = BroadcastNotification<SignedBlock>;

  using SendSignedVote = BroadcastNotification<SignedVote>;
  using SignedVoteReceived = GossipNotificationReceived<SignedVote>;
}  // namespace lean::messages
