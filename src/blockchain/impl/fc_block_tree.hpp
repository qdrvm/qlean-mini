/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/shared_ref.hpp>

#include "blockchain/block_tree.hpp"
#include "blockchain/fork_choice.hpp"

namespace lean::blockchain {

  class FCBlockTree final : public BlockTree {
   public:
    FCBlockTree(qtils::SharedRef<ForkChoiceStore> fork_choice_store);

    ~FCBlockTree() override = default;

    const BlockHash &getGenesisBlockHash() const override;

    bool has(const BlockHash &hash) const override;

    outcome::result<BlockHeader> getBlockHeader(
        const BlockHash &block_hash) const override;

    outcome::result<std::optional<BlockHeader>> tryGetBlockHeader(
        const BlockHash &block_hash) const override;

    outcome::result<BlockBody> getBlockBody(
        const BlockHash &block_hash) const override;

    outcome::result<void> addBlockHeader(const BlockHeader &header) override;

    outcome::result<void> addBlock(
        SignedBlockWithAttestation signed_block_with_attestation) override;

    outcome::result<void> removeLeaf(const BlockHash &block_hash) override;

    outcome::result<void> addExistingBlock(
        const BlockHash &block_hash, const BlockHeader &block_header) override;

    outcome::result<void> addBlockBody(const BlockHash &block_hash,
                                       const BlockBody &body) override;

    outcome::result<void> finalize(const BlockHash &block_hash,
                                   const Justification &justification) override;

    outcome::result<std::vector<BlockHash>> getBestChainFromBlock(
        const BlockHash &block, uint64_t maximum) const override;

    outcome::result<std::vector<BlockHash>> getDescendingChainToBlock(
        const BlockHash &block, uint64_t maximum) const override;

    bool isFinalized(const BlockIndex &block) const override;

    BlockIndex bestBlock() const override;

    outcome::result<BlockIndex> getBestContaining(
        const BlockHash &target_hash) const override;

    std::vector<BlockHash> getLeaves() const override;
    // std::vector<BlockIndex> getLeavesInfo() const override;

    outcome::result<std::vector<BlockHash>> getChildren(
        const BlockHash &block) const override;

    BlockIndex lastFinalized() const override;

    outcome::result<std::optional<SignedBlockWithAttestation>>
    tryGetSignedBlock(const BlockHash block_hash) const override;
    void import(std::vector<SignedBlockWithAttestation> blocks) override;

    // BlockHeaderRepository methods

    outcome::result<Slot> getSlotByHash(
        const BlockHash &block_hash) const override;

   private:
    std::shared_ptr<ForkChoiceStore> fork_choice_store_;
  };

}  // namespace lean::blockchain
