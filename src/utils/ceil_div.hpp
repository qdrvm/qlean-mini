/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <concepts>

namespace lean {
  auto ceilDiv(const std::integral auto &l, const std::integral auto &r) {
    return (l + r - 1) / r;
  }
}  // namespace lean
