/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/block_hash.hpp"
#include "types/slot.hpp"

namespace lean {
  struct BlockIndexRef {
    Slot slot;
    const BlockHash &hash;
  };
}  // namespace lean

template <>
struct fmt::formatter<lean::BlockIndexRef> {
  // Presentation format
  bool long_form = true;

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
  auto format(const lean::BlockIndexRef &v, FormatContext &ctx) const
      -> decltype(ctx.out()) {
    [[unlikely]] if (long_form) {
      return fmt::format_to(ctx.out(), "{:0xx} @ {}", v.hash, v.slot);
    }
    return fmt::format_to(ctx.out(), "{:0x} @ {}", v.hash, v.slot);
  }
};
