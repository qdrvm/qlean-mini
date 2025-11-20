/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "types/attestations.hpp"

namespace lean {
  struct BlockBody : ssz::ssz_variable_size_container {
    /// @note attestations will be replaced by aggregated attestations.
    Attestations attestations;

    SSZ_CONT(attestations);
    bool operator==(const BlockBody &) const = default;
  };

}  // namespace lean
