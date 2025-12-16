/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "serde/json_fwd.hpp"

namespace lean {
  struct Config : ssz::ssz_container {
    uint64_t genesis_time;

    SSZ_CONT(genesis_time);

    bool operator==(const Config &) const = default;

    JSON_CAMEL(genesis_time);
  };
}  // namespace lean
