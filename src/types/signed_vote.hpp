/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/vote.hpp"

namespace lean {

  struct SignedVote : ssz::ssz_container {
    uint64_t validator_id = 0;
    Vote data;
    /// @note The signature type is still to be determined so Bytes32 is used in
    /// the interim. The actual signature size is expected to be a lot larger
    /// (~3 KiB).
    qtils::ByteArr<4000> signature;

    SSZ_CONT(validator_id, data, signature);
  };

}  // namespace lean
