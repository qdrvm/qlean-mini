/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/outcome.hpp>

namespace qtils {
  /**
   * Get value or `outcome::raise`.
   * Same as `.value()`.
   */
  template <typename T>
  auto valueOrRaise(outcome::result<T> r) {
    if (not r.has_value()) {
      raise(r.error());
    }
    return std::move(r).value();
  }
}  // namespace qtils
