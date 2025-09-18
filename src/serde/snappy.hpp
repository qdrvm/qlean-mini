/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <snappy.h>

#include <qtils/bytes.hpp>
#include <qtils/bytestr.hpp>

namespace lean {
  enum class SnappyError {
    UNCOMPRESS_TOO_LONG,
    UNCOMPRESS_INVALID,
  };
  Q_ENUM_ERROR_CODE(SnappyError) {
    using E = decltype(e);
    switch (e) {
      case E::UNCOMPRESS_TOO_LONG:
        return "SnappyError::UNCOMPRESS_TOO_LONG";
      case E::UNCOMPRESS_INVALID:
        return "SnappyError::UNCOMPRESS_INVALID";
    }
    abort();
  }

  inline qtils::ByteVec snappyCompress(qtils::BytesIn input) {
    std::string compressed;
    snappy::Compress(qtils::byte2str(input.data()), input.size(), &compressed);
    return qtils::ByteVec{qtils::str2byte(std::as_const(compressed))};
  }

  inline outcome::result<qtils::ByteVec> snappyUncompress(
      qtils::BytesIn compressed, size_t max_size = 4 << 20) {
    size_t size = 0;
    if (not snappy::GetUncompressedLength(
            qtils::byte2str(compressed.data()), compressed.size(), &size)) {
      return SnappyError::UNCOMPRESS_INVALID;
    }
    if (size > max_size) {
      return SnappyError::UNCOMPRESS_TOO_LONG;
    }
    std::string uncompressed;
    if (not snappy::Uncompress(qtils::byte2str(compressed.data()),
                               compressed.size(),
                               &uncompressed)) {
      return SnappyError::UNCOMPRESS_INVALID;
    }
    return qtils::ByteVec{qtils::str2byte(std::as_const(uncompressed))};
  }
}  // namespace lean
