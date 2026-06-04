/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <limits>

namespace lean {
  template <typename A, typename B, typename R = decltype(A{} + B{})>
  R saturatingAdd(const A &a, const B &b) {
    constexpr auto max = std::numeric_limits<R>::max();
    auto max_b = max - a;
    return b < max_b ? a + b : 0;
  }

  auto saturatingSub(const auto &a, const auto &b) {
    return a >= b ? a - b : 0;
  }
}  // namespace lean
