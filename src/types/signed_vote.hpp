/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/vote.hpp"
#include "types/vote_signature.hpp"

namespace lean {

  struct SignedVote : ssz::ssz_container {
    uint64_t validator_id = 0;
    Vote data;
    VoteSignature signature;

    SSZ_CONT(validator_id, data, signature);
  };

}  // namespace lean
