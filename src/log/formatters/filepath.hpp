/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>

#include <fmt/core.h>

template <>
struct fmt::formatter<std::filesystem::path>
    : fmt::formatter<std::string_view> {
  auto format(const std::filesystem::path &path, format_context &ctx) const {
    return fmt::formatter<std::string_view>::format(path.native(), ctx);
  }
};
