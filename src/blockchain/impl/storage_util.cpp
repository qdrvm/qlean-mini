/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/impl/storage_util.hpp"

#include <qtils/visit_in_place.hpp>

#include "storage/storage_error.hpp"

using qtils::ByteVec;
// using lean::Hash256;
// using lean::primitives::BlockId;
// using lean::primitives::BlockNumber;
// using lean::storage::Space;

namespace lean::blockchain {

  outcome::result<std::optional<qtils::ByteVecOrView>> blockIdToBlockHash(
      storage::SpacedStorage &storage, const BlockId &block_id) {
    return visit_in_place(
        block_id,
        [&](const BlockNumber &block_number)
            -> outcome::result<std::optional<qtils::ByteVecOrView>> {
          auto key_space = storage.getSpace(storage::Space::LookupKey);
          return key_space->tryGet(slotToHashLookupKey(block_number));
        },
        [](const BlockHash &block_hash) {
          return std::make_optional(ByteVec(block_hash));
        });
  }

  outcome::result<std::optional<BlockHash>> blockHashByNumber(
      storage::SpacedStorage &storage, BlockNumber block_number) {
    auto key_space = storage.getSpace(storage::Space::LookupKey);
    OUTCOME_TRY(data_opt, key_space->tryGet(slotToHashLookupKey(block_number)));
    if (data_opt.has_value()) {
      OUTCOME_TRY(hash, BlockHash::fromSpan(data_opt.value()));
      return hash;
    }
    return std::nullopt;
  }

  outcome::result<bool> hasInSpace(storage::SpacedStorage &storage,
                                   storage::Space space,
                                   const BlockId &block_id) {
    OUTCOME_TRY(key, blockIdToBlockHash(storage, block_id));
    if (not key.has_value()) {
      return false;
    }

    auto target_space = storage.getSpace(space);
    return target_space->contains(key.value());
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

}  // namespace lean::blockchain
