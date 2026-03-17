/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "serde/json_fwd.hpp"
#include "types/signed_block_with_attestation.hpp"

namespace lean {
  struct StatusMessage : ssz::ssz_container {
    Checkpoint finalized;
    Checkpoint head;

    SSZ_AND_JSON_FIELDS(finalized, head);
  };

  struct BlockRequest : ssz::ssz_variable_size_container {
    ssz::list<BlockHash, MAX_REQUEST_BLOCKS> roots;

    SSZ_AND_JSON_FIELDS(roots);
  };

  using BlockResponse = SignedBlockWithAttestation;
}  // namespace lean
