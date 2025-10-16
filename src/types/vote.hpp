/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <types/checkpoint.hpp>

namespace lean {

  struct Vote : public ssz::ssz_container {
    uint64_t slot = 0;
    Checkpoint head;
    Checkpoint target;
    Checkpoint source;

    SSZ_CONT(slot, head, target, source);
  };

}  // namespace lean
