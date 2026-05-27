/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

namespace lean {
  inline std::optional<std::string_view> getEnv(const std::string &name) {
    auto *s = getenv(name.c_str());
    if (s == nullptr) {
      return std::nullopt;
    }
    return s;
  }
}  // namespace lean
