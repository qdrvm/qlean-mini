/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/lists.hpp>

namespace lean {
  using LeanAggregatedSignature = ssz::list<uint8_t, 1024 * 1024>;
}  // namespace lean
