/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <types/checkpoint.hpp>

namespace lean {

  struct Vote : public ssz::ssz_container {
    uint64_t validator_id;
    uint64_t slot;
    Checkpoint head;
    Checkpoint target;
    Checkpoint source;

    SSZ_CONT(validator_id, slot, head, target, source);
  };

}  // namespace lean
