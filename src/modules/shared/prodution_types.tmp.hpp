/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/block_header.hpp"
#include "types/types.hpp"

namespace lean::messages {

  struct SlotStarted {
    Slot slot;
    Epoch epoch;
    bool epoch_change;
  };

  struct SlotIntervalOneStarted {
    Slot slot;
    Epoch epoch;
  };

  struct SlotIntervalTwoStarted {
    Slot slot;
    Epoch epoch;
  };

  struct SlotIntervalThreeStarted {
    Slot slot;
    Epoch epoch;
  };

  struct NewLeaf {
    BlockHeader header;
    bool best = false;
  };

  struct Finalized {
    BlockIndex finalized;
    std::vector<BlockIndex> retired;
  };

}  // namespace lean::messages
