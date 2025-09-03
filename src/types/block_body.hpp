/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "types/constants.hpp"
#include "types/vote.hpp"

namespace lean {

  struct BlockBody : ssz::ssz_container {
    /// @note votes will be replaced by aggregated attestations.
    ssz::list<Vote, VALIDATOR_REGISTRY_LIMIT> votes;

    SSZ_CONT(votes);
  };

}  // namespace lean
