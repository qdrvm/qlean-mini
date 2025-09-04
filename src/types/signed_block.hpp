/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/block.hpp"
#include <qtils/byte_arr.hpp>

namespace lean {

  struct SignedBlock {
    Block message;
    qtils::ByteArr<32> signature;
  };

}  // namespace lean
