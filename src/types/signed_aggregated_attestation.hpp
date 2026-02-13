/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "types/aggregated_signature_proof.hpp"
#include "types/attestation_data.hpp"

namespace lean {
  /**
   * A signed aggregated attestation for broadcasting.
   * Contains the attestation data and the aggregated signature proof.
   */
  struct SignedAggregatedAttestation : ssz::ssz_variable_size_container {
    AttestationData data;
    AggregatedSignatureProof proof;

    SSZ_CONT(data, proof);
    bool operator==(const SignedAggregatedAttestation &) const = default;
  };
}  // namespace lean
