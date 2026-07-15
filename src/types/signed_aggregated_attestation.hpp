/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "serde/json_fwd.hpp"
#include "types/attestation_data.hpp"
#include "types/type_one_multi_signature.hpp"

namespace lean {
  /**
   * A signed aggregated attestation for broadcasting.
   * Contains the attestation data and the aggregated signature proof.
   */
  struct SignedAggregatedAttestation : ssz::ssz_variable_size_container {
    AttestationData data;
    TypeOneMultiSignature proof;

    SSZ_AND_JSON_FIELDS(data, proof);
    bool operator==(const SignedAggregatedAttestation &) const = default;
  };
}  // namespace lean
