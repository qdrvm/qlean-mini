/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/lists.hpp>

namespace lean {
  using ByteList512KiB = ssz::list<uint8_t, 512 << 10>;
}  // namespace lean
