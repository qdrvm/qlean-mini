/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/vote.hpp"

namespace lean {

  struct SignedVote : ssz::ssz_container {
    Vote data;
    /// @note The signature type is still to be determined so Bytes32 is used in
    /// the interim. The actual signature size is expected to be a lot larger
    /// (~3 KiB).
    qtils::ByteArr<32> signature;

    SSZ_CONT(data, signature);
  };

}  // namespace lean
