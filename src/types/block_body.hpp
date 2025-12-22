/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "types/aggregated_attestations.hpp"

namespace lean {
  struct BlockBody : ssz::ssz_variable_size_container {
    AggregatedAttestations attestations;

    SSZ_CONT(attestations);
    bool operator==(const BlockBody &) const = default;
  };

}  // namespace lean
