/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/peer/peer_id.hpp>

#include "types/types.hpp"

namespace lean::messages {

  struct BlockDiscoveredMessage {
    BlockIndex index;
    libp2p::PeerId peer;
  };

}  // namespace lean::messages
