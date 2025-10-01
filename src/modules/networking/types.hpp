/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/signed_block.hpp"

namespace lean {
  struct StatusMessage : ssz::ssz_container {
    Checkpoint finalized;
    Checkpoint head;

    SSZ_CONT(finalized, head);
  };

  struct BlockRequest : ssz::ssz_variable_size_container {
    ssz::list<BlockHash, MAX_REQUEST_BLOCKS> blocks;

    SSZ_CONT(blocks);
  };

  struct BlockResponse : ssz::ssz_variable_size_container {
    ssz::list<SignedBlock, MAX_REQUEST_BLOCKS> blocks;

    SSZ_CONT(blocks);
  };
}  // namespace lean
