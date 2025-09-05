/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/types.hpp"

namespace lean {

  struct BlockIndex {
    Slot slot;
    BlockHash hash;
    auto operator<=>(const BlockIndex &other) const = default;
  };

}  // namespace lean

template <>
struct std::hash<lean::BlockIndex> {
  std::size_t operator()(const lean::BlockIndex &s) const noexcept {
    std::size_t h = std::hash<lean::Slot>{}(s.slot);
    for (auto b : s.hash) {
      h ^= std::hash<std::uint8_t>{}(b) + 0x9e3779b97f4a7c15ULL + (h << 6)
         + (h >> 2);
    }
    return h;
  }
};

template <>
struct fmt::formatter<lean::BlockIndex> {
  // Presentation format
  bool long_form = false;

  // Parses format specifications of the form ['s' | 'l'].
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end) {
      if (*it == 'l' or *it == 's') {
        long_form = *it == 'l';
        ++it;
      }
    }
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  // Formats the BlockIndex using the parsed format specification (presentation)
  // stored in this formatter.
  template <typename FormatContext>
  auto format(const lean::BlockIndex &block_index, FormatContext &ctx) const
      -> decltype(ctx.out()) {
    [[unlikely]] if (long_form) {
      return fmt::format_to(
          ctx.out(), "{:0xx} @ {}", block_index.hash, block_index.slot);
    }
    return fmt::format_to(
        ctx.out(), "{:0x} @ {}", block_index.hash, block_index.slot);
  }
};
