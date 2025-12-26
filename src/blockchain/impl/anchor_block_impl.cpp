/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/impl/anchor_block_impl.hpp"

#include "types/state.hpp"

namespace lean::blockchain {

  AnchorBlockImpl::AnchorBlockImpl(const AnchorState &state) {
    slot = state.slot;
    proposer_index = 0;
    parent_root = kZeroHash;
    state_root = sszHash(state);
    body = BlockBody{};
  }

}  // namespace lean::blockchain
