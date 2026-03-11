/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/checkpoint.hpp"
#include "types/validator_index.hpp"

namespace lean {
  struct ForkChoiceNodeApiJson {
    BlockHash root;
    Slot slot;
    BlockHash parent_root;
    ValidatorIndex proposer_index;
    ValidatorIndex weight;

    JSON_FIELDS(root, slot, parent_root, proposer_index, weight);
  };

  struct ForkChoiceApiJson {
    std::vector<ForkChoiceNodeApiJson> nodes;
    BlockHash head;
    Checkpoint justified;
    Checkpoint finalized;
    BlockHash safe_target;
    ValidatorIndex validator_count;

    JSON_FIELDS(
        nodes, head, justified, finalized, safe_target, validator_count);
  };
}  // namespace lean
