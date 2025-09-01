/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <blake2.h>
#include <qtils/bytes.hpp>
#include <qtils/byte_arr.hpp>

namespace lean::crypto {
  struct Blake {
    using Hash = qtils::ByteArr<32>;
    blake2b_state state;
    Blake() {
      blake2b_init(&state, sizeof(Hash));
    }
    Blake &update(qtils::BytesIn input) {
      blake2b_update(&state, input.data(), input.size());
      return *this;
    }
    Hash hash() const {
      Hash hash;
      auto state2 = state;
      blake2b_final(&state2, hash.data(), sizeof(Hash));
      return hash;
    }
    static Hash hash(qtils::BytesIn input) {
      return Blake{}.update(input).hash();
    }
  };
}  // namespace lean
