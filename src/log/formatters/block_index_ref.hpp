/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <type_traits>

#include "types/block_hash.hpp"
#include "types/slot.hpp"

namespace lean {

  struct BlockIndexRef {
    Slot slot;
    const BlockHash &hash;
  };

}  // namespace lean

/**
 * Formatter for lean::BlockIndexRef.
 *
 * Presentation specifier:
 *   - 'l' (default): long form (full hash)
 *   - 's'          : short form (abbreviated hash)
 *
 * Optional fmt-like alignment and width:
 *   {:<20}        : left align (default)
 *   {:^20}        : center
 *   {:>20}        : right
 *   {:<{};s}      : runtime width + short form
 *
 * Notes:
 *   - Hash size is assumed to be fixed and obtained via BlockHash::size().
 *   - Width is computed in bytes of the produced UTF-8 output
 *     (consistent with existing Hex formatter behavior).
 */
template <>
struct fmt::formatter<lean::BlockIndexRef> {
  // Presentation format: long ('l') or short ('s').
  bool long_form = true;

  // Optional fmt-like alignment and width.
  char fill = ' ';
  char align = '<';  // default: left
  int width = 0;

  // Dynamic width support: "{:<{}}" or "{:<{0}}".
  bool dynamic_width = false;
  int width_arg_id = -1;

  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin();
    auto end = ctx.end();

    auto fail = [&] {
      throw format_error(
          "invalid format for BlockIndexRef: optional [fill][align][width|{arg}]"
          "[;] then optional ['l'|'s']");
    };

    auto is_align = [](char c) {
      return c == '<' || c == '>' || c == '^';
    };
    auto is_digit = [](char c) {
      return c >= '0' && c <= '9';
    };

    // ---- Optional [fill][align] ----
    if (it != end && *it != '}' && (it + 1) != end && it[1] != '}' &&
        is_align(it[1])) {
      // "<fill><align>"
      fill = *it;
      align = it[1];
      it += 2;
    } else if (it != end && *it != '}' && is_align(*it)) {
      // "<align>"
      align = *it;
      ++it;
    }

    // ---- Optional width (static or dynamic) ----
    if (it != end && *it >= '1' && *it <= '9') {
      int w = 0;
      while (it != end && is_digit(*it)) {
        w = w * 10 + (*it - '0');
        ++it;
      }
      width = w;
    } else if (it != end && *it == '{') {
      // Dynamic width: "{}" or "{n}"
      ++it;  // skip '{'
      if (it == end) {
        fail();
      }

      dynamic_width = true;

      if (*it == '}') {
        // "{}" -> next automatic argument index
        width_arg_id = ctx.next_arg_id();
        ++it;
      } else {
        // "{n}" -> explicit argument index
        int id = 0;
        if (!is_digit(*it)) {
          fail();
        }
        while (it != end && is_digit(*it)) {
          id = id * 10 + (*it - '0');
          ++it;
        }
        if (it == end || *it != '}') {
          fail();
        }
        ctx.check_arg_id(id);
        width_arg_id = id;
        ++it;
      }
    }

    // Optional separator between fmt-part and presentation part.
    if (it != end && *it == ';') {
      ++it;
    }

    // ---- Optional presentation specifier ----
    if (it != end && *it != '}') {
      if (*it == 'l' || *it == 's') {
        long_form = (*it == 'l');
        ++it;
      } else {
        fail();
      }
    }

    if (it != end && *it != '}') {
      fail();
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const lean::BlockIndexRef &v, FormatContext &ctx) const
      -> decltype(ctx.out()) {
    auto out = ctx.out();

    // Resolve runtime width if dynamic width was used.
    int w = width;
    if (dynamic_width) {
      auto arg = ctx.arg(width_arg_id);

      int dyn = 0;
      bool ok = false;

      // Non-deprecated fmt API.
      arg.visit([&](auto &&val) {
        using T = std::remove_cvref_t<decltype(val)>;
        if constexpr (std::is_integral_v<T>) {
          dyn = static_cast<int>(val);
          ok = true;
        }
      });

      if (!ok) {
        throw format_error(
            "BlockIndexRef: dynamic width argument is not an integer");
      }

      w = (dyn > 0) ? dyn : 0;
    }

    // ---- Compute output length in bytes ----
    //
    // Hash length:
    //   long  -> "0x" + (N * 2 hex chars)
    //   short -> "0x" + head(2B->4 chars) + ellipsis(3 bytes) + tail(2B->4 chars)
    static constexpr int kEllipsisBytes = 3;  // UTF-8 '…'
    static constexpr int kAtLen = 3;          // " @ "
    static constexpr size_t kHead = 2;
    static constexpr size_t kTail = 2;

    const int hash_bytes = static_cast<int>(v.hash.size());

    const int hash_len = long_form
                             ? (2 + hash_bytes * 2)
                             : (2 + static_cast<int>(kHead * 2) +
                                kEllipsisBytes +
                                static_cast<int>(kTail * 2));

    // Slot length in decimal digits.
    const uint64_t slot_val = static_cast<uint64_t>(v.slot);
    int slot_len = 1;
    uint64_t tmp = slot_val;
    while (tmp >= 10) {
      tmp /= 10;
      ++slot_len;
    }

    const int len = hash_len + kAtLen + slot_len;
    const int pad = (w > len) ? (w - len) : 0;

    auto write_content = [&] {
      if (long_form) {
        out = fmt::format_to(out, "{:0xx} @ {}", v.hash, v.slot);
      } else {
        out = fmt::format_to(out, "{:0x} @ {}", v.hash, v.slot);
      }
    };

    // ---- Alignment handling ----
    if (align == '<') {
      write_content();
      for (int i = 0; i < pad; ++i) {
        *out++ = fill;
      }
      return out;
    }

    if (align == '>') {
      for (int i = 0; i < pad; ++i) {
        *out++ = fill;
      }
      write_content();
      return out;
    }

    // Center alignment.
    if (pad <= 0) {
      write_content();
      return out;
    }

    const int left = pad / 2;
    const int right = pad - left;

    for (int i = 0; i < left; ++i) {
      *out++ = fill;
    }
    write_content();
    for (int i = 0; i < right; ++i) {
      *out++ = fill;
    }
    return out;
  }
};