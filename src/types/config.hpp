/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

namespace lean {
  struct Config : ssz::ssz_container {
    /// @note temporary property to support simplified round robin block
    /// production in absence of randao & deposit mechanisms
    uint64_t num_validators;
    uint64_t genesis_time;

    SSZ_CONT(num_validators, genesis_time);
  };
}  // namespace lean
