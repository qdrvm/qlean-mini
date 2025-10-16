/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/ssz++.hpp>

#include "log/formatters/block_index_ref.hpp"

namespace lean {

  struct Checkpoint : ssz::ssz_container {
    BlockHash root;
    Slot slot = 0;

    static Checkpoint from(const auto &v) {
      return Checkpoint{.root = v.hash(), .slot = v.slot};
    }

    SSZ_CONT(root, slot);
  };

}  // namespace lean

template <>
struct fmt::formatter<lean::Checkpoint> : fmt::formatter<lean::BlockIndexRef> {
  template <typename FormatContext>
  auto format(const lean::Checkpoint &v, FormatContext &ctx) const
      -> decltype(ctx.out()) {
    return fmt::formatter<lean::BlockIndexRef>::format(
        lean::BlockIndexRef{v.slot, v.root}, ctx);
  }
};
