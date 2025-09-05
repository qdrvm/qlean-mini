/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cinttypes>

#include <qtils/byte_arr.hpp>
#include <qtils/tagged.hpp>

namespace lean {
  // stub types. must be refactored in future

  struct Stub {};

  // blockchain types

  using OpaqueHash = qtils::ByteArr<32>;

  using BlockHash = OpaqueHash;
  using HeaderHash = OpaqueHash;
  using StateRoot = OpaqueHash;
  using BodyRoot = OpaqueHash;

  using Slot = uint64_t;
  using Epoch = uint64_t;  // is needed?

  using ProposerIndex = uint64_t;

  // networking types

  using PeerId = qtils::Tagged<Stub, struct PeerId_>;  // STUB

  // /// Direction, in which to retrieve ordered data
  // enum class Direction : uint8_t {
  //   /// from child to parent
  //   ASCENDING = 0,
  //   /// from parent to canonical child
  //   DESCENDING = 1
  // };
  //
  // /// Request for blocks to another peer
  // struct BlocksRequest {
  //   /// start from this block
  //   BlockIndex from{};
  //   /// sequence direction
  //   Direction direction{};
  //   /// maximum number of blocks to return; an implementation defined maximum
  //   is
  //   /// used when unspecified
  //   std::optional<uint32_t> max{};
  //   bool multiple_justifications = true;
  // };
  //
  // struct BlockAnnounce {
  //   BlockAnnounce(const BlockAnnounce &) = delete;
  // };

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
