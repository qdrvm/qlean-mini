/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "types/checkpoint.hpp"
#include "types/slot.hpp"

namespace lean {
  struct AttestationData : ssz::ssz_container {
    Slot slot;
    Checkpoint head;
    Checkpoint target;
    Checkpoint source;

    SSZ_CONT(slot, head, target, source);
    bool operator==(const AttestationData &) const = default;
  };
}  // namespace lean
