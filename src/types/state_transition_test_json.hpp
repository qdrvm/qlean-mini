/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "serde/json_fwd.hpp"
#include "types/block.hpp"
#include "types/state.hpp"

namespace lean {
  struct StateExpectation {
    Slot slot;
    std::optional<Slot> latest_justified_slot;
    std::optional<BlockHash> latest_justified_root;
    std::optional<Slot> latest_finalized_slot;
    std::optional<BlockHash> latest_finalized_root;
    std::optional<size_t> validator_count;
    std::optional<Slot> latest_block_header_slot;
    std::optional<BlockHash> latest_block_header_state_root;
    std::optional<size_t> historical_block_hashes_count;

    JSON_CAMEL(slot,
               latest_justified_slot,
               latest_justified_root,
               latest_finalized_slot,
               latest_finalized_root,
               validator_count,
               latest_block_header_slot,
               latest_block_header_state_root,
               historical_block_hashes_count);
  };

  struct StateTransitionTestJson {
    State pre;
    std::vector<Block> blocks;
    std::optional<StateExpectation> post;
    std::optional<std::string> expect_exception;

    JSON_CAMEL(pre, blocks, post, expect_exception);
  };
}  // namespace lean
