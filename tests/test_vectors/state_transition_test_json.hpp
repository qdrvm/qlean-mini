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
    std::optional<uint64_t> config_genesis_time;
    std::optional<Slot> latest_block_header_slot;
    std::optional<ValidatorIndex> latest_block_header_proposer_index;
    std::optional<BlockHash> latest_block_header_parent_root;
    std::optional<BlockHash> latest_block_header_state_root;
    std::optional<BlockHash> latest_block_header_body_root;
    std::optional<size_t> historical_block_hashes_count;
    std::optional<decltype(State::historical_block_hashes)>
        historical_block_hashes;
    std::optional<decltype(State::justified_slots)> justified_slots;
    std::optional<decltype(State::justifications_roots)> justifications_roots;
    std::optional<decltype(State::justifications_validators)>
        justifications_validators;

    JSON_FIELDS(slot,
                latest_justified_slot,
                latest_justified_root,
                latest_finalized_slot,
                latest_finalized_root,
                validator_count,
                config_genesis_time,
                latest_block_header_slot,
                latest_block_header_proposer_index,
                latest_block_header_parent_root,
                latest_block_header_state_root,
                latest_block_header_body_root,
                historical_block_hashes_count,
                historical_block_hashes,
                justified_slots,
                justifications_roots,
                justifications_validators);
  };

  struct StateTransitionTestJson {
    State pre;
    std::vector<Block> blocks;
    std::optional<StateExpectation> post;
    std::optional<std::string> expect_exception;

    JSON_FIELDS(pre, blocks, post, expect_exception);
  };
}  // namespace lean
