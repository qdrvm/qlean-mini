/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/slot.hpp"
#include "types/validator_index.hpp"

namespace lean {
  inline bool isProposer(ValidatorIndex validator_index,
                         Slot slot,
                         ValidatorIndex num_validators) {
    return slot % num_validators == validator_index;
  }
}  // namespace lean
