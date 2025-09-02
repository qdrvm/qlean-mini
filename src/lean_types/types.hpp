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

  using ProposerIndex = uint64_t;

  struct BlockIndex {
    Slot slot;
    BlockHash hash;
    auto operator<=>(const BlockIndex &other) const = default;
  };

  using BlockInfo = BlockIndex;

  using BlockNumber = Slot;

  using BlockId = std::variant<Slot, BlockHash>;

  // networking types

  using PeerId = qtils::Tagged<Stub, struct PeerId_>;  // STUB

  /// Direction, in which to retrieve ordered data
  enum class Direction : uint8_t {
    /// from child to parent
    ASCENDING = 0,
    /// from parent to canonical child
    DESCENDING = 1
  };

  /// Request for blocks to another peer
  struct BlocksRequest {
    /// start from this block
    BlockIndex from{};
    /// sequence direction
    Direction direction{};
    /// maximum number of blocks to return; an implementation defined maximum is
    /// used when unspecified
    std::optional<uint32_t> max{};
    bool multiple_justifications = true;
  };

  struct BlockAnnounce {
    BlockAnnounce(const BlockAnnounce &) = delete;
  };

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

template <>
struct fmt::formatter<lean::BlockInfo> {
  // Presentation format: 's' - short, 'l' - long.
  char presentation = 's';

  // Parses format specifications of the form ['s' | 'l'].
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    // Parse the presentation format and store it in the formatter:
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && (*it == 's' or *it == 'l')) {
      presentation = *it++;
    }

    // Check if reached the end of the range:
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }

    // Return an iterator past the end of the parsed range:
    return it;
  }

  // Formats the BlockInfo using the parsed format specification (presentation)
  // stored in this formatter.
  template <typename FormatContext>
  auto format(const lean::BlockInfo &block_info, FormatContext &ctx) const
      -> decltype(ctx.out()) {
    // ctx.out() is an output iterator to write to.

    if (presentation == 's') {
      return fmt::format_to(
          ctx.out(), "{:0x} @ {}", block_info.hash, block_info.slot);
    }

    return fmt::format_to(
        ctx.out(), "{:0xx} @ {}", block_info.hash, block_info.slot);
  }
};


template <typename T, typename U>
struct fmt::formatter<qtils::Tagged<T, U>> : formatter<T> {};
