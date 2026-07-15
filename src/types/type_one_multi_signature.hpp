/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "serde/json_fwd.hpp"
#include "types/aggregation_bits.hpp"
#include "types/byte_list_512_kib.hpp"

namespace lean {
  struct TypeOneMultiSignature : ssz::ssz_variable_size_container {
    AggregationBits participants;
    ByteList512KiB proof;

    SSZ_AND_JSON_FIELDS(participants, proof);
    bool operator==(const TypeOneMultiSignature &) const = default;
  };
}  // namespace lean
