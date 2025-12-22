/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "types/block_signatures.hpp"
#include "types/block_with_attestation.hpp"

namespace lean {
  struct SignedBlockWithAttestation : ssz::ssz_variable_size_container {
    BlockWithAttestation message;
    BlockSignatures signature;

    SSZ_CONT(message, signature);
    bool operator==(const SignedBlockWithAttestation &) const = default;
  };
}  // namespace lean
