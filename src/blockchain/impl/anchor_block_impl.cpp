/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/impl/anchor_block_impl.hpp"

#include "types/state.hpp"

namespace lean::blockchain {

  AnchorBlockImpl::AnchorBlockImpl(const AnchorState &state) {
    BlockHeader::operator=(state.latest_block_header);
    state_root = sszHash(state);
    updateHash();
  }

}  // namespace lean::blockchain
