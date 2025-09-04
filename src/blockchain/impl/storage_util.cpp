/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/impl/storage_util.hpp"

#include <qtils/visit_in_place.hpp>

#include "storage/storage_error.hpp"

using qtils::ByteVec;
// using jam::Hash256;
// using jam::primitives::BlockId;
// using jam::primitives::Slot;
// using jam::storage::Space;

namespace lean::blockchain {

  outcome::result<bool> hasInSpace(storage::SpacedStorage &storage,
                                   storage::Space space,
                                   const BlockHash &block_hash) {
    auto target_space = storage.getSpace(space);
    return target_space->contains(block_hash);
  }

  outcome::result<void> putToSpace(storage::SpacedStorage &storage,
                                   storage::Space space,
                                   const BlockHash &block_hash,
                                   qtils::ByteVecOrView &&value) {
    auto target_space = storage.getSpace(space);
    return target_space->put(block_hash, std::move(value));
  }

  outcome::result<std::optional<qtils::ByteVecOrView>> getFromSpace(
      storage::SpacedStorage &storage,
      storage::Space space,
      const BlockHash &block_hash) {
    auto target_space = storage.getSpace(space);
    return target_space->tryGet(block_hash);
  }

  outcome::result<void> removeFromSpace(storage::SpacedStorage &storage,
                                        storage::Space space,
                                        const BlockHash &block_hash) {
    auto target_space = storage.getSpace(space);
    return target_space->remove(block_hash);
  }

}  // namespace jam::blockchain
