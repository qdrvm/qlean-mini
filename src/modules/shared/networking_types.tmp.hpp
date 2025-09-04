/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/block.hpp"
#include "types/block_header.hpp"
#include "types/types.hpp"
#include "utils/request_id.hpp"

namespace lean::messages {

  struct PeerConnectedMessage {
    PeerId peer;
    // address?
    // initial view?
  };

  struct PeerDisconnectedMessage {
    PeerId peer;
    // reason?
  };

  struct BlockAnnounce {
    BlockHeader header;
    PeerId peer;
  };

  struct BlockAnnounceMessage {
    BlockAnnounce header;
    PeerId peer;
  };

  struct BlockRequestMessage {
    RequestCxt ctx;
//    BlocksRequest request;
    PeerId peer;
  };

  struct BlockResponseMessage {
    RequestCxt ctx;
    outcome::result<Block> result;
    PeerId peer;
  };

}  // namespace lean::messages
