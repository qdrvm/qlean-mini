/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "serde/json_fwd.hpp"
#include "types/aggregation_bits.hpp"
#include "types/attestation_data.hpp"

namespace lean {
  struct AggregatedAttestation : ssz::ssz_variable_size_container {
    AggregationBits aggregation_bits;
    AttestationData data;

    SSZ_AND_JSON_FIELDS(aggregation_bits, data);
    bool operator==(const AggregatedAttestation &) const = default;
  };
}  // namespace lean
