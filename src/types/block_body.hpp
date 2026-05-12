/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "serde/json_fwd.hpp"
#include "types/aggregated_attestations.hpp"

namespace lean {
  struct BlockBody : ssz::ssz_variable_size_container {
    AggregatedAttestations attestations;

    SSZ_AND_JSON_FIELDS(attestations);
    bool operator==(const BlockBody &) const = default;
  };

}  // namespace lean
