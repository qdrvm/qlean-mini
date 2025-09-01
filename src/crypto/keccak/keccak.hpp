/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/byte_arr.hpp>

#include "keccak/keccak.h"

namespace lean::crypto {
  inline Hash256 keccak(qtils::ByteView buf) {
    Hash256 out;
    sha3_HashBuffer(256,
                    SHA3_FLAGS::SHA3_FLAGS_KECCAK,
                    buf.data(),
                    buf.size(),
                    out.data(),
                    32);
    return out;
  }
}  // namespace lean::crypto
