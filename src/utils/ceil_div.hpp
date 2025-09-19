/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

namespace lean {
  auto ceilDiv(const auto &l, const auto &r) {
    return (l + r - 1) / r;
  }
}  // namespace lean
