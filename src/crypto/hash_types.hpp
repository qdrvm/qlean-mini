/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/byte_arr.hpp>

namespace lean {
  using Hash64 = qtils::ByteArr<8>;
  using Hash128 = qtils::ByteArr<16>;
  using Hash256 = qtils::ByteArr<32>;
  using Hash512 = qtils::ByteArr<64>;
}  // namespace lean
