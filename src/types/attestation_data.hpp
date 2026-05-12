/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "serde/json_fwd.hpp"
#include "types/checkpoint.hpp"
#include "types/slot.hpp"

namespace lean {
  struct AttestationData : ssz::ssz_container {
    Slot slot;
    Checkpoint head;
    Checkpoint target;
    Checkpoint source;

    SSZ_AND_JSON_FIELDS(slot, head, target, source);
    bool operator==(const AttestationData &) const = default;
  };
}  // namespace lean
