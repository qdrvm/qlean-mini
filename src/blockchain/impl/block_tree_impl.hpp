/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <thread>

#include <qtils/final_action.hpp>

#include "blockchain/block_storage.hpp"
#include "blockchain/block_tree.hpp"
#include "blockchain/impl/block_tree_initializer.hpp"
#include "blockchain/impl/cached_tree.hpp"
#include "log/logger.hpp"
#include "metrics/histogram_timer.hpp"
#include "se/impl/common.hpp"
#include "se/subscription.hpp"
#include "se/subscription_fwd.hpp"

namespace lean::metrics {
  class Registry;
}
namespace lean::app {
  class Configuration;
}

namespace lean::blockchain {
  class BlockTreeInitializer;
}

namespace lean::crypto {
  class Hasher;
}  // namespace lean::crypto

namespace lean::blockchain {

  class BlockTreeImpl final
      : public BlockTree,
        public std::enable_shared_from_this<BlockTreeImpl> {
   public:
    BlockTreeImpl(qtils::SharedRef<log::LoggingSystem> logsys,
                  qtils::SharedRef<BlockStorage> storage,
                  qtils::SharedRef<crypto::Hasher> hasher,
                  std::shared_ptr<Subscription> se_manager,
                  qtils::SharedRef<BlockTreeInitializer> initializer);

    ~BlockTreeImpl() override = default;

    const BlockHash &getGenesisBlockHash() const override;

    bool has(const BlockHash &hash) const override;

    outcome::result<BlockHeader> getBlockHeader(
        const BlockHash &block_hash) const override;

    outcome::result<std::optional<BlockHeader>> tryGetBlockHeader(
        const BlockHash &block_hash) const override;

    outcome::result<BlockBody> getBlockBody(
        const BlockHash &block_hash) const override;

    // outcome::result<Justification> getBlockJustification(
    //       const BlockHash &block_hash) const override;

    outcome::result<void> addBlockHeader(const BlockHeader &header) override;

    outcome::result<void> addBlock(const Block &block) override;

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

    // outcome::result<std::vector<BlockHash>> getChainByBlocks(
    //     const BlockHash &ancestor,
    //     const BlockHash &descendant) const override;
    //
    // bool hasDirectChain(const BlockHash &ancestor,
    //                     const BlockHash &descendant) const override;

    bool isFinalized(const BlockIndex &block) const override;

    BlockIndex bestBlock() const override;

    outcome::result<BlockIndex> getBestContaining(
        const BlockHash &target_hash) const override;

    std::vector<BlockHash> getLeaves() const override;
    // std::vector<BlockIndex> getLeavesInfo() const override;

    outcome::result<std::vector<BlockHash>> getChildren(
        const BlockHash &block) const override;

    BlockIndex lastFinalized() const override;

    StatusMessage getStatusMessage() const override;
    outcome::result<std::optional<SignedBlock>> tryGetSignedBlock(
        const BlockHash block_hash) const override;
    void import(std::vector<SignedBlock> blocks) override;

    // BlockHeaderRepository methods

    outcome::result<Slot> getNumberByHash(
        const BlockHash &block_hash) const override;

    // outcome::result<BlockHash> getHashByNumber(
    //     Slot slot) const override;

   private:
    struct BlockTreeData {
      qtils::SharedRef<BlockStorage> storage_;
      // std::shared_ptr<storage::trie_pruner::TriePruner> state_pruner_;
      std::unique_ptr<CachedTree> tree_;
      qtils::SharedRef<crypto::Hasher> hasher_;
      // std::shared_ptr<const class JustificationStoragePolicy>
      //     justification_storage_policy_;
      std::optional<BlockHash> genesis_block_hash_;
      // BlocksPruning blocks_pruning_;
    };

    outcome::result<void> reorgAndPrune(const BlockTreeData &p,
                                        const ReorgAndPrune &changes);

    outcome::result<BlockHeader> getBlockHeaderNoLock(
        const BlockTreeData &p, const BlockHash &block_hash) const;

    //   outcome::result<void> pruneTrie(const BlockTreeData &block_tree_data,
    //                                   Slot new_finalized);

    BlockIndex getLastFinalizedNoLock(const BlockTreeData &p) const;
    BlockIndex bestBlockNoLock(const BlockTreeData &p) const;

    // bool hasDirectChainNoLock(const BlockTreeData &p,
    //                           const BlockHash &ancestor,
    //                           const BlockHash &descendant);
    std::vector<BlockHash> getLeavesNoLock(const BlockTreeData &p) const;

    outcome::result<std::vector<BlockHash>> getDescendingChainToBlockNoLock(
        const BlockTreeData &p,
        const BlockHash &to_block,
        uint64_t maximum) const;

    outcome::result<void> addExistingBlockNoLock(
        BlockTreeData &p,
        const BlockHash &block_hash,
        const BlockHeader &block_header);

    class SafeBlockTreeData {
     public:
      explicit SafeBlockTreeData(BlockTreeData data);

      template <typename F>
      decltype(auto) exclusiveAccess(F &&f) {
        // if this thread owns the mutex, it shall
        // not be unlocked until this function exits
        if (exclusive_owner_.load(std::memory_order_acquire)
            == std::this_thread::get_id()) {
          return f(block_tree_data_.unsafeGet());
        }
        return block_tree_data_.exclusiveAccess(
            [&f, this](BlockTreeData &data) {
              exclusive_owner_ = std::this_thread::get_id();
              qtils::FinalAction reset([&] {
                exclusive_owner_ = decltype(std::this_thread::get_id()){};
              });
              return f(data);
            });
      }

      template <typename F>
      decltype(auto) sharedAccess(F &&f) const {
        // if this thread owns the mutex, it shall
        // not be unlocked until this function exits
        if (exclusive_owner_.load(std::memory_order_acquire)
            == std::this_thread::get_id()) {
          return f(block_tree_data_.unsafeGet());
        }
        return block_tree_data_.sharedAccess(std::forward<F>(f));
      }

     private:
      se::utils::SafeObject<BlockTreeData> block_tree_data_;
      std::atomic<std::thread::id> exclusive_owner_ =
          decltype(std::this_thread::get_id()){};
    };

    log::Logger log_;
    std::shared_ptr<Subscription> se_manager_;

    SafeBlockTreeData block_tree_data_;

    // Metrics
    metrics::GaugeHelper metric_best_block_height_;
    metrics::GaugeHelper metric_finalized_block_height_;
    metrics::GaugeHelper metric_known_chain_leaves_;
  };

}  // namespace lean::blockchain
