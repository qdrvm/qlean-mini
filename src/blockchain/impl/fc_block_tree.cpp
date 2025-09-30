/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/impl/fc_block_tree.hpp"

#include "blockchain/block_tree_error.hpp"
#include "types/signed_block.hpp"

namespace lean::blockchain {
  FCBlockTree::FCBlockTree(qtils::SharedRef<ForkChoiceStore> fork_choice_store)
      : fork_choice_store_(std::move(fork_choice_store)) {}

  const BlockHash &FCBlockTree::getGenesisBlockHash() const {
    throw std::runtime_error("FCBlockTree::getGenesisBlockHash()");
  }

  bool FCBlockTree::has(const BlockHash &hash) const {
    return fork_choice_store_->hasBlock(hash);
  }

  outcome::result<BlockHeader> FCBlockTree::getBlockHeader(
      const BlockHash &block_hash) const {
    throw std::runtime_error("FCBlockTree::getBlockHeader()");
  }

  outcome::result<std::optional<BlockHeader>> FCBlockTree::tryGetBlockHeader(
      const BlockHash &block_hash) const {
    throw std::runtime_error("FCBlockTree::tryGetBlockHeader()");
  }

  outcome::result<BlockBody> FCBlockTree::getBlockBody(
      const BlockHash &block_hash) const {
    throw std::runtime_error("FCBlockTree::getBlockBody()");
  }

  outcome::result<void> FCBlockTree::addBlockHeader(const BlockHeader &header) {
    throw std::runtime_error("FCBlockTree::addBlockHeader()");
  }

  outcome::result<void> FCBlockTree::addBlock(const Block &block) {
    return fork_choice_store_->onBlock(block);
  }

  outcome::result<void> FCBlockTree::removeLeaf(const BlockHash &block_hash) {
    throw std::runtime_error("FCBlockTree::removeLeaf()");
  }

  outcome::result<void> FCBlockTree::addExistingBlock(
      const BlockHash &block_hash, const BlockHeader &block_header) {
    throw std::runtime_error("FCBlockTree::addExistingBlock()");
  }

  outcome::result<void> FCBlockTree::addBlockBody(const BlockHash &block_hash,
                                                  const BlockBody &body) {
    throw std::runtime_error("FCBlockTree::addBlockBody()");
  }

  outcome::result<void> FCBlockTree::finalize(
      const BlockHash &block_hash, const Justification &justification) {
    throw std::runtime_error("FCBlockTree::finalize()");
  }

  outcome::result<std::vector<BlockHash>> FCBlockTree::getBestChainFromBlock(
      const BlockHash &block, uint64_t maximum) const {
    throw std::runtime_error("FCBlockTree::getBestChainFromBlock()");
  }

  outcome::result<std::vector<BlockHash>>
  FCBlockTree::getDescendingChainToBlock(const BlockHash &block,
                                         uint64_t maximum) const {
    throw std::runtime_error("FCBlockTree::getDescendingChainToBlock()");
  }

  bool FCBlockTree::isFinalized(const BlockIndex &block) const {
    throw std::runtime_error("FCBlockTree::isFinalized()");
  }

  BlockIndex FCBlockTree::bestBlock() const {
    return BlockIndex{.slot = fork_choice_store_->getHeadSlot(),
                      .hash = fork_choice_store_->getHead()};
  }

  outcome::result<BlockIndex> FCBlockTree::getBestContaining(
      const BlockHash &target_hash) const {
    throw std::runtime_error("FCBlockTree::getBestContaining()");
  }

  std::vector<BlockHash> FCBlockTree::getLeaves() const {
    throw std::runtime_error("FCBlockTree::getLeaves()");
  }
  // std::vector<BlockIndex> getLeavesInfo() const override;

  outcome::result<std::vector<BlockHash>> FCBlockTree::getChildren(
      const BlockHash &block) const {
    throw std::runtime_error("FCBlockTree::getChildren()");
  }

  BlockIndex FCBlockTree::lastFinalized() const {
    auto finalized = fork_choice_store_->getLatestFinalized();
    return BlockIndex{.slot = finalized.slot, .hash = finalized.root};
  }

  outcome::result<std::optional<SignedBlock>> FCBlockTree::tryGetSignedBlock(
      const BlockHash block_hash) const {
    throw std::runtime_error("FCBlockTree::tryGetSignedBlock()");
  }

  void FCBlockTree::import(std::vector<SignedBlock> blocks) {}

  // BlockHeaderRepository methods

  outcome::result<Slot> FCBlockTree::getNumberByHash(
      const BlockHash &block_hash) const {
    auto opt = fork_choice_store_->getBlockSlot(block_hash);
    if (not opt.has_value()) {
      return BlockTreeError::HEADER_NOT_FOUND;
    }
    return opt.value();
  }
}  // namespace lean::blockchain
