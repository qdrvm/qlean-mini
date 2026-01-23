/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "types/attestations_signatures.hpp"
#include "types/signature.hpp"

namespace lean {
  struct BlockSignatures : ssz::ssz_variable_size_container {
    AttestationSignatures attestation_signatures;
    Signature proposer_signature;

    SSZ_CONT(attestation_signatures, proposer_signature);
    bool operator==(const BlockSignatures &) const = default;
  };
}  // namespace lean
