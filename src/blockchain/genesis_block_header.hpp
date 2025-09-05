/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/block_header.hpp"

namespace lean::blockchain {

  class GenesisBlockHeader : public BlockHeader {
   public:
    using BlockHeader::BlockHeader;
  };

}  // namespace lean::blockchain
