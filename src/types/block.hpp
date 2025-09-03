/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/byte_arr.hpp>

#include "types/block_body.hpp"

namespace lean {

  struct Block {
    uint64_t slot;
    uint64_t proposer_index;
    qtils::ByteArr<32> parent_root;
    qtils::ByteArr<32> state_root;
    BlockBody body;
  };

}  // namespace lean
