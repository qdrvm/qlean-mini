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

  /**
   * Stub method to sign block.
   */
  inline SignedBlock signBlock(Block block) {
    return SignedBlock{.message = std::move(block)};
  }
}  // namespace lean
