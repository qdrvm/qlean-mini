/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/byte_arr.hpp>
#include <sszpp/ssz++.hpp>

#include "types.hpp"

namespace lean {

  struct Checkpoint : ssz::ssz_container {
    qtils::ByteArr<32> root;
    Slot slot;

    SSZ_CONT(root, slot);
  };

}  // namespace lean
