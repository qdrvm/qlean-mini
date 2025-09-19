/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cmath>

#include <boost/assert.hpp>

#include "types/slot.hpp"

namespace lean {
  inline bool isJustifiableSlot(Slot finalized_slot, Slot candidate) {
    BOOST_ASSERT(candidate >= finalized_slot);
    auto delta = candidate - finalized_slot;
    return delta <= 5
        // any x^2
        or fmod(sqrt(delta), 1) == 0
        // any x^2+x
        or fmod(sqrt(delta + 0.25), 1) == 0.5;
  }
}  // namespace lean
