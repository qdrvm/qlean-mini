/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cinttypes>

#include <qtils/byte_arr.hpp>
#include <qtils/tagged.hpp>

#include "types/block_hash.hpp"
#include "types/slot.hpp"

namespace lean {
  // stub types. must be refactored in future

  struct Stub {};

  // blockchain types

  using OpaqueHash = qtils::ByteArr<32>;

  using HeaderHash = OpaqueHash;
  using StateRoot = OpaqueHash;
  using BodyRoot = OpaqueHash;

  using Epoch = uint64_t;  // is needed?

  using ProposerIndex = uint64_t;
}  // namespace lean

template <>
struct fmt::formatter<lean::Stub> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const lean::Stub &, FormatContext &ctx) const
      -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "stub");
  }
};

template <typename T, typename U>
struct fmt::formatter<qtils::Tagged<T, U>> : formatter<T> {};
