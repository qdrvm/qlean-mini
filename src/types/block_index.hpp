/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "log/formatters/block_index_ref.hpp"
#include "types/checkpoint.hpp"

namespace lean {

  /**
   * Type like Checkpoint with different members order
   */
  struct BlockIndex {
    Slot slot;
    BlockHash hash;
    auto operator<=>(const BlockIndex &other) const = default;

    // NOLINTNEXTLINE(google-explicit-constructor)
    explicit(false) operator Checkpoint() const noexcept {
      return {.root = hash, .slot = slot};
    }
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
struct fmt::formatter<lean::BlockIndex> : fmt::formatter<lean::BlockIndexRef> {
  template <typename FormatContext>
  auto format(const lean::BlockIndex &block_index, FormatContext &ctx) const
      -> decltype(ctx.out()) {
    return fmt::formatter<lean::BlockIndexRef>::format(
        lean::BlockIndexRef{block_index.slot, block_index.hash}, ctx);
  }
};
