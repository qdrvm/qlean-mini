/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "serde/serialization.hpp"
#include "serde/snappy.hpp"

namespace lean {
  auto encodeSszSnappy(const auto &t) {
    return snappy::compress(encode(t).value());
  }

  template <typename T>
  outcome::result<T> decodeSszSnappy(qtils::BytesIn compressed) {
    BOOST_OUTCOME_TRY(auto uncompressed, snappy::uncompress(compressed));
    return decode<T>(uncompressed);
  }

  auto encodeSszSnappyFramed(const auto &t) {
    return snappy::compressFramed(encode(t).value());
  }

  template <typename T>
  outcome::result<T> decodeSszSnappyFramed(qtils::BytesIn compressed) {
    BOOST_OUTCOME_TRY(auto uncompressed, snappy::uncompressFramed(compressed));
    return decode<T>(uncompressed);
  }
}  // namespace lean
