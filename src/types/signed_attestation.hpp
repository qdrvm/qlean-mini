/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "serde/json_fwd.hpp"
#include "types/attestation_data.hpp"
#include "types/signature.hpp"
#include "types/validator_index.hpp"

namespace lean {
  struct SignedAttestation : ssz::ssz_container {
    ValidatorIndex validator_id;
    AttestationData data;
    Signature signature;

    static SignedAttestation from(const auto &attestation,
                                  const auto &signature) {
      return SignedAttestation{
          .validator_id = attestation.validator_id,
          .data = attestation.data,
          .signature = signature,
      };
    }

    SSZ_AND_JSON_FIELDS(validator_id, data, signature);
  };
}  // namespace lean
