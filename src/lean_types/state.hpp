/**
* Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

namespace lean {

  struct State {

    Config: config;
    uint64_t: slot;
    BlockHeader: latest_block_header;

    Checkpoint: latest_justified;
    Checkpoint: latest_finalized;

    List[Bytes32, HISTORICAL_ROOTS_LIMIT]: historical_block_hashes;
    List[bool, HISTORICAL_ROOTS_LIMIT]: justified_slots;

    // Diverged from 3SF-mini.py:
    // Flattened `justifications: Dict[str, List[bool]]` for SSZ compatibility
    List[Bytes32, HISTORICAL_ROOTS_LIMIT]: justifications_roots;
    Bitlist[
        HISTORICAL_ROOTS_LIMIT * VALIDATOR_REGISTRY_LIMIT
    ]: justifications_validators;

  };

}  // namespace lean

