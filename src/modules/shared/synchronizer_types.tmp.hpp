/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "lean_types/types.hpp"

namespace lean::messages {

  struct BlockDiscoveredMessage {
    BlockIndex index;
    PeerId peer;
  };

}