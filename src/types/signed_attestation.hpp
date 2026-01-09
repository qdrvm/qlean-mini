/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "types/attestation_data.hpp"
#include "types/signature.hpp"
#include "types/validator_index.hpp"

namespace lean {
  struct SignedAttestation : ssz::ssz_container {
    ValidatorIndex validator_id;
    AttestationData message;
    Signature signature;

    SSZ_CONT(validator_id, message, signature);
  };
}  // namespace lean
