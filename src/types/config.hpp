/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

namespace lean {
  struct Config : ssz::ssz_container {
    uint64_t genesis_time;
    uint64_t subnet_count;

    SSZ_CONT(genesis_time, subnet_count);

    bool operator==(const Config &) const = default;
  };
}  // namespace lean
