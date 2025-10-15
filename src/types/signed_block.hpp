/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/block.hpp"
#include "types/block_signature.hpp"

namespace lean {

  struct SignedBlock : ssz::ssz_variable_size_container {
    Block message;
    BlockSignature signature;

    SSZ_CONT(message, signature);
  };

}  // namespace lean
