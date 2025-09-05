/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/shared_ref.hpp>

#include "types/block_index.hpp"

namespace lean::blockchain {
  using BlockWeight = std::tuple<Slot>;

  /**
   * Used to update hashes of best chain by number.
   */
  struct Reorg {
    BlockIndex common;
    std::vector<BlockIndex> revert;
    std::vector<BlockIndex> apply;

    [[nodiscard]] bool empty() const;
  };

  /**
   * Used to enqueue blocks for removal.
   * Children are removed before parent.
   */
  struct ReorgAndPrune {
    std::optional<Reorg> reorg;
    std::vector<BlockIndex> prune;
  };

  /**
   * In-memory light representation of the tree, used for efficiency and usage
   * convenience - we would only ask the database for some info, when directly
   * requested
   */
  class TreeNode {
   public:
    explicit TreeNode(const BlockIndex &info);
    TreeNode(const BlockIndex &info, const qtils::SharedRef<TreeNode> &parent);

    [[nodiscard]] std::shared_ptr<TreeNode> parent() const;
    void detach();
    [[nodiscard]] BlockWeight weight() const;

    const BlockIndex index;

   private:
    std::weak_ptr<TreeNode> weak_parent;

   public:
    uint32_t depth = 0;
    bool contains_approved_para_block = false;
    bool reverted = false;  // TODO Looks like actually unused

    std::vector<qtils::SharedRef<TreeNode>> children{};
  };

  Reorg reorg(qtils::SharedRef<TreeNode> from, qtils::SharedRef<TreeNode> to);

  bool canDescend(const qtils::SharedRef<TreeNode> &from,
                  const qtils::SharedRef<TreeNode> &to);

  /**
   * Non-finalized part of the block tree
   */
  class CachedTree {
   public:
    explicit CachedTree(const BlockIndex &root);

    [[nodiscard]] BlockIndex finalized() const;
    [[nodiscard]] BlockIndex best() const;
    [[nodiscard]] size_t leafCount() const;
    [[nodiscard]] std::vector<BlockHash> leafHashes() const;
    [[nodiscard]] std::vector<BlockIndex> leafInfo() const;
    [[nodiscard]] bool isLeaf(const BlockHash &hash) const;
    [[nodiscard]] BlockIndex bestWith(
        const qtils::SharedRef<TreeNode> &required) const;
    [[nodiscard]] std::optional<qtils::SharedRef<TreeNode>> find(
        const BlockHash &hash) const;
    std::optional<Reorg> add(const qtils::SharedRef<TreeNode> &new_node);
    ReorgAndPrune finalize(const qtils::SharedRef<TreeNode> &new_finalized);

    /**
     * Can't remove finalized root.
     */
    ReorgAndPrune removeLeaf(const BlockHash &hash);

    /**
     * Used when switching from fast-sync to full-sync.
     */
    ReorgAndPrune removeUnfinalized();

    /// Force find and update the actual best block
    void forceRefreshBest();

   private:
    /**
     * Compare node weight with best and replace if heavier.
     * @return true if heavier and replaced.
     */
    bool chooseBest(const qtils::SharedRef<TreeNode> &node);

    qtils::SharedRef<TreeNode> root_;
    qtils::SharedRef<TreeNode> best_;
    std::unordered_map<BlockHash, qtils::SharedRef<TreeNode>> nodes_;
    std::unordered_map<BlockHash, Slot> leaves_;
  };
}  // namespace lean::blockchain
