/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "lean_types/constants.hpp"
#include "lean_types/vote.hpp"

namespace lean {

  struct BlockBody {
    /// @note votes will be replaced by aggregated attestations.
    std::array<Vote, VALIDATOR_REGISTRY_LIMIT> votes;
  };

}  // namespace lean
