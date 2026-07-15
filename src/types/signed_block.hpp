/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "serde/json_fwd.hpp"
#include "types/block.hpp"
#include "types/type_two_multi_signature.hpp"

namespace lean {
  struct SignedBlock : ssz::ssz_variable_size_container {
    Block block;
    TypeTwoMultiSignature proof;

    SSZ_AND_JSON_FIELDS(block, proof);
    bool operator==(const SignedBlock &) const = default;
  };
}  // namespace lean
