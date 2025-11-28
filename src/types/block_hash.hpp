/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/byte_arr.hpp>

namespace lean {
  using BlockHash = qtils::ByteArr<32>;

  constexpr BlockHash kZeroHash;
}  // namespace lean
