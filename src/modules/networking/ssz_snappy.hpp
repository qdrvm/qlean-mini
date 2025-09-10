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
    return snappyCompress(encode(t).value());
  }

  template <typename T>
  outcome::result<T> decodeSszSnappy(qtils::BytesIn compressed) {
    BOOST_OUTCOME_TRY(auto uncompressed, snappyUncompress(compressed));
    return decode<T>(uncompressed);
  }
}  // namespace lean
