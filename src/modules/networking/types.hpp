/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "serde/json_fwd.hpp"
#include "types/signed_block.hpp"

namespace lean {
  struct StatusMessage : ssz::ssz_container {
    Checkpoint finalized;
    Checkpoint head;

    SSZ_AND_JSON_FIELDS(finalized, head);
  };

  struct BlocksByRootRequest : ssz::ssz_variable_size_container {
    ssz::list<BlockHash, MAX_REQUEST_BLOCKS> roots;

    SSZ_AND_JSON_FIELDS(roots);
  };

  struct BlocksByRangeRequest : ssz::ssz_variable_size_container {
    Slot start_slot;
    Slot count;

    SSZ_AND_JSON_FIELDS(start_slot, count);
  };

  using BlockResponse = SignedBlock;
  using BlocksResponse = std::vector<SignedBlock>;
}  // namespace lean
