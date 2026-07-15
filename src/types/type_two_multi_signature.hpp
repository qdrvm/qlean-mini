/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "serde/json_fwd.hpp"
#include "types/byte_list_512_kib.hpp"

namespace lean {
  struct TypeTwoMultiSignature : ssz::ssz_variable_size_container {
    ByteList512KiB proof;

    SSZ_AND_JSON_FIELDS(proof);
    bool operator==(const TypeTwoMultiSignature &) const = default;
  };
}  // namespace lean
