/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include "types/block.hpp"
#include "utils/ctor_limiters.hpp"

namespace lean {
  struct AnchorState;
}

namespace lean::blockchain {

  class AnchorBlockImpl final : public AnchorBlock, Singleton<AnchorBlock> {
   public:
    AnchorBlockImpl(const AnchorState &state);
  };

}  // namespace lean::blockchain
