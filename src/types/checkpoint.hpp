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
    Slot slot = 0;

    static Checkpoint from(const auto &v) {
      return Checkpoint{.root = v.hash(), .slot = v.slot};
    }

    SSZ_CONT(root, slot);
  };

}  // namespace lean
