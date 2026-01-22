/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "types/aggregation_bits.hpp"
#include "types/lean_aggregated_signature.hpp"

namespace lean {
  struct AggregatedSignatureProof : ssz::ssz_variable_size_container {
    AggregationBits participants;
    LeanAggregatedSignature proof_data;

    SSZ_CONT(participants, proof_data);
  };
}  // namespace lean
