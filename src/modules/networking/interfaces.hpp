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

    virtual void dispatch_block_announce(
        std::shared_ptr<const messages::BlockAnnounceMessage> msg) = 0;

    virtual void dispatch_block_response(
        std::shared_ptr<const messages::BlockResponseMessage> msg) = 0;
  };

  struct Networking {
    virtual ~Networking() = default;

    virtual void on_loaded_success() = 0;

    virtual void on_loading_is_finished() = 0;

    virtual void on_block_request(
        std::shared_ptr<const messages::BlockRequestMessage> msg) = 0;
  };

}  // namespace lean::modules