/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "types/constants.hpp"
#include "types/signed_vote.hpp"

namespace lean {

  using Attestations = ssz::list<SignedVote, VALIDATOR_REGISTRY_LIMIT>;
  struct BlockBody : ssz::ssz_variable_size_container {
    /// @note votes will be replaced by aggregated attestations.
    Attestations attestations;

    SSZ_CONT(attestations);
  };

}  // namespace lean
