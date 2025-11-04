/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/block_header.hpp"
#include "types/checkpoint.hpp"
#include "types/config.hpp"
#include "types/constants.hpp"
#include "types/validators.hpp"

namespace lean {
  struct State : ssz::ssz_variable_size_container {
    Config config;
    Slot slot;
    BlockHeader latest_block_header;

    Checkpoint latest_justified;
    Checkpoint latest_finalized;

    ssz::list<BlockHash, HISTORICAL_ROOTS_LIMIT> historical_block_hashes;
    ssz::list<bool, HISTORICAL_ROOTS_LIMIT> justified_slots;

    Validators validators;

    // Diverged from 3SF-mini.py:
    // Flattened `justifications: Dict[str, List[bool]]` for SSZ compatibility
    ssz::list<BlockHash, HISTORICAL_ROOTS_LIMIT> justifications_roots;
    ssz::list<bool, HISTORICAL_ROOTS_LIMIT * VALIDATOR_REGISTRY_LIMIT>
        justifications_validators;

    SSZ_CONT(config,
             slot,
             latest_block_header,
             latest_justified,
             latest_finalized,
             historical_block_hashes,
             justified_slots,
             justifications_roots,
             justifications_validators);
    bool operator==(const State &) const = default;

    ValidatorIndex validatorCount() const {
      return config.num_validators;
    }
  };

  using AnchorState = qtils::Tagged<State, struct AnchorStateTag>;
}  // namespace lean
