/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/validator_index.hpp"
#include "types/vote.hpp"
#include "types/vote_signature.hpp"

namespace lean {

  struct SignedVote : ssz::ssz_container {
    ValidatorIndex validator_id = 0;
    Vote data;
    VoteSignature signature;

    SSZ_CONT(validator_id, data, signature);
  };

  /**
   * Stub method to sign vote.
   */
  inline SignedVote signVote(ValidatorIndex validator_id, Vote vote) {
    return SignedVote{.validator_id = validator_id, .data = std::move(vote)};
  }
}  // namespace lean
