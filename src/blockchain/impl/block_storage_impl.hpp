/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "blockchain/block_storage.hpp"
#include "blockchain/impl/block_storage_initializer.hpp"
#include "log/logger.hpp"
#include "storage/spaced_storage.hpp"

namespace lean::blockchain {

  class BlockStorageImpl : public BlockStorage, Singleton<BlockStorage> {
   public:
    friend class BlockStorageInitializer;

    BlockStorageImpl(qtils::SharedRef<log::LoggingSystem> logsys,
                     qtils::SharedRef<storage::SpacedStorage> storage,
                     qtils::SharedRef<crypto::Hasher> hasher,
                     std::shared_ptr<BlockStorageInitializer>);

    ~BlockStorageImpl() override = default;

    outcome::result<void> setBlockTreeLeaves(
        std::vector<BlockHash> leaves) override;

    outcome::result<std::vector<BlockHash>> getBlockTreeLeaves() const override;

    outcome::result<BlockIndex> getLastFinalized() const override;

    // -- hash --

    outcome::result<void> assignHashToSlot(const BlockIndex &block) override;

    outcome::result<void> deassignHashToSlot(
        const BlockIndex &block_index) override;

    outcome::result<std::vector<BlockHash>> getBlockHash(
        Slot slot) const override;

    outcome::result<SlotIterator> seekLastSlot() const override;

    // -- header --

    outcome::result<bool> hasBlockHeader(
        const BlockHash &block_hash) const override;

    outcome::result<BlockHash> putBlockHeader(
        const BlockHeader &header) override;

    outcome::result<BlockHeader> getBlockHeader(
        const BlockHash &block_hash) const override;

    outcome::result<std::optional<BlockHeader>> tryGetBlockHeader(
        const BlockHash &block_hash) const override;

    // -- body --

    outcome::result<void> putBlockBody(const BlockHash &block_hash,
                                       const BlockBody &block_body) override;

    outcome::result<std::optional<BlockBody>> getBlockBody(
        const BlockHash &block_hash) const override;

    outcome::result<void> removeBlockBody(const BlockHash &block_hash) override;

    // -- justification --

    outcome::result<void> putJustification(
        const Justification &justification,
        const BlockHash &block_hash) override;

    outcome::result<std::optional<Justification>> getJustification(
        const BlockHash &block_hash) const override;

    outcome::result<void> removeJustification(
        const BlockHash &block_hash) override;

    // -- combined

    outcome::result<BlockHash> putBlock(const BlockData &block) override;

    outcome::result<std::optional<SignedBlock>> getBlock(
        const BlockHash &block_hash) const override;

    outcome::result<void> removeBlock(const BlockHash &block_hash) override;

   private:
    outcome::result<std::optional<BlockHeader>> fetchBlockHeader(
        const BlockHash &block_hash) const;

    log::Logger logger_;

    qtils::SharedRef<storage::SpacedStorage> storage_;

    std::shared_ptr<crypto::Hasher> hasher_;

    mutable std::optional<std::vector<BlockHash>> block_tree_leaves_;
  };
}  // namespace lean::blockchain
