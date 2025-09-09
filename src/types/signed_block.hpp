/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/byte_arr.hpp>

#include "types/block.hpp"

namespace lean {

  struct SignedBlock : ssz::ssz_container {
    Block message;
    qtils::ByteArr<32> signature;

    SSZ_CONT(message, signature);
  };

}  // namespace lean
