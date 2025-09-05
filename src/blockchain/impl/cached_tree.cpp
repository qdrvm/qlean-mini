/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/impl/cached_tree.hpp"

#include <deque>
#include <ranges>
#include <set>

#include <qtils/shared_ref.hpp>

namespace lean::blockchain {
  bool Reorg::empty() const {
    return revert.empty() and apply.empty();
  }

  TreeNode::TreeNode(const BlockIndex &info) : index{info} {}

  TreeNode::TreeNode(const BlockIndex &info,
                     const qtils::SharedRef<TreeNode> &parent)
      : index{info},
        weak_parent{std::shared_ptr<TreeNode>(parent)},
        depth(parent->depth + 1),
        reverted{parent->reverted} {}

  std::shared_ptr<TreeNode> TreeNode::parent() const {
    return weak_parent.lock();
  }

  void TreeNode::detach() {
    return weak_parent.reset();
  }

  BlockWeight TreeNode::weight() const {
    return index.slot;
  }

  Reorg reorg(qtils::SharedRef<TreeNode> from, qtils::SharedRef<TreeNode> to) {
    Reorg reorg;
    while (from != to) {
      if (from->index.slot > to->index.slot) {
        reorg.revert.emplace_back(from->index);
        from = from->parent();
        [[unlikely]] if (not from) { throw std::bad_weak_ptr{}; }

      } else {
        reorg.apply.emplace_back(to->index);
        to = to->parent();
        [[unlikely]] if (not from) { throw std::bad_weak_ptr{}; }
      }
    }
    reorg.common = to->index;
    std::ranges::reverse(reorg.apply);
    return reorg;
  }

  template <typename F>
  bool descend(qtils::SharedRef<TreeNode> from,
               const qtils::SharedRef<TreeNode> &to,
               const F &f) {
    while (from != to) {
      if (from->index.slot <= to->index.slot) {
        return false;
      }
      f(from);
      from = from->parent();
      [[unlikely]] if (not from) { throw std::bad_weak_ptr{}; }
    }
    return true;
  }

  bool canDescend(const qtils::SharedRef<TreeNode> &from,
                  const qtils::SharedRef<TreeNode> &to) {
    return descend(from, to, [](const qtils::SharedRef<TreeNode> &) {});
  }

  bool CachedTree::chooseBest(const qtils::SharedRef<TreeNode> &node) {
    if (node->reverted) {
      return false;
    }
    BOOST_ASSERT(not best_->reverted);
    if (node->weight() > best_->weight()) {
      best_ = node;
      return true;
    }
    return false;
  }

  struct Cmp {
    bool operator()(const qtils::SharedRef<TreeNode> &lhs,
                    const qtils::SharedRef<TreeNode> &rhs) const {
      BOOST_ASSERT(lhs and rhs);
      return rhs->index.slot < lhs->index.slot;
    }
  };

  void CachedTree::forceRefreshBest() {
    std::set<qtils::SharedRef<TreeNode>, Cmp> candidates;
    for (const auto &key : leaves_ | std::views::keys) {
      auto node = find(key);
      BOOST_ASSERT(node.has_value());
      candidates.emplace(std::move(node.value()));
    }

    best_ = root_;
    while (not candidates.empty()) {
      auto node = candidates.extract(candidates.begin());
      BOOST_ASSERT(not node.empty());

      auto &tree_node = node.value();
      if (tree_node->reverted) {
        if (auto parent = tree_node->parent()) {
          candidates.emplace(std::move(parent));
        }
        continue;
      }

      if (best_->weight() < tree_node->weight()) {
        best_ = tree_node;
      }
    }
  }

  CachedTree::CachedTree(const BlockIndex &root)
      : root_{std::make_shared<TreeNode>(root)},
        best_{root_},
        nodes_{{root.hash, root_}} {
    leaves_.emplace(root.hash, root.slot);
  }

  BlockIndex CachedTree::finalized() const {
    return root_->index;
  }

  BlockIndex CachedTree::best() const {
    return best_->index;
  }

  size_t CachedTree::leafCount() const {
    return leaves_.size();
  }

  std::vector<BlockIndex> CachedTree::leafInfo() const {
    std::vector<BlockIndex> output;
    output.reserve(leaves_.size());
    std::ranges::transform(
        leaves_, std::back_inserter(output), [](const auto &v) {
          return BlockIndex(v.second, v.first);
        });
    return output;
  }

  std::vector<BlockHash> CachedTree::leafHashes() const {
    std::vector<BlockHash> output;
    output.reserve(leaves_.size());
    std::ranges::transform(leaves_,
                           std::back_inserter(output),
                           [](const auto &v) { return v.first; });
    return output;
  }

  bool CachedTree::isLeaf(const BlockHash &hash) const {
    return leaves_.contains(hash);
  }

  BlockIndex CachedTree::bestWith(
      const qtils::SharedRef<TreeNode> &required) const {
    std::set<qtils::SharedRef<TreeNode>, Cmp> candidates;
    for (const auto &leaf : leaves_ | std::views::keys) {
      auto node_opt = find(leaf);
      BOOST_ASSERT(node_opt.has_value());
      candidates.emplace(std::move(node_opt.value()));
    }
    auto best = required;
    while (not candidates.empty()) {
      auto _node = candidates.extract(candidates.begin());
      auto &node = _node.value();
      if (node->index.slot <= required->index.slot) {
        continue;
      }
      if (node->reverted) {
        if (auto parent = node->parent()) {
          candidates.emplace(std::move(parent));
        }
        continue;
      }
      if (node->weight() > best->weight()) {
        if (canDescend(node, required)) {
          best = node;
        }
      }
    }
    return best->index;
  }

  std::optional<qtils::SharedRef<TreeNode>> CachedTree::find(
      const BlockHash &hash) const {
    if (auto it = nodes_.find(hash); it != nodes_.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  std::optional<Reorg> CachedTree::add(
      const qtils::SharedRef<TreeNode> &new_node) {
    if (nodes_.contains(new_node->index.hash)) {
      return std::nullopt;
    }
    BOOST_ASSERT(new_node->children.empty());
    auto parent = new_node->parent();
    [[unlikely]] if (not parent) { throw std::bad_weak_ptr{}; }
    BOOST_ASSERT(not std::ranges::contains(parent->children, new_node));

    new_node->depth = parent->depth + 1;

    parent->children.emplace_back(new_node);
    nodes_.emplace(new_node->index.hash, new_node);
    leaves_.erase(parent->index.hash);
    leaves_.emplace(new_node->index.hash, new_node->index.slot);
    if (not new_node->reverted and new_node->weight() > best_->weight()) {
      auto old_best = best_;
      best_ = new_node;
      return reorg(old_best, best_);
    }
    return std::nullopt;
  }

  ReorgAndPrune CachedTree::finalize(
      const qtils::SharedRef<TreeNode> &new_finalized) {
    BOOST_ASSERT(new_finalized->index.slot >= root_->index.slot);
    if (new_finalized == root_) {
      return {};
    }
    BOOST_ASSERT(new_finalized->parent());
    ReorgAndPrune changes;
    if (not canDescend(best_, new_finalized)) {
      changes.reorg = reorg(best_, new_finalized);
    }
    std::deque<std::shared_ptr<TreeNode>> queue;
    for (std::shared_ptr<TreeNode> finalized_child = new_finalized,
                                   parent = finalized_child->parent();
         parent;
         finalized_child = parent, parent = parent->parent()) {
      for (auto &child : parent->children) {
        if (child == finalized_child) {
          continue;
        }
        queue.emplace_back(child);
      }
      parent->children.clear();
      nodes_.erase(parent->index.hash);
    }
    while (not queue.empty()) {
      auto parent = std::move(queue.front());
      queue.pop_front();
      changes.prune.emplace_back(parent->index);
      for (auto &child : parent->children) {
        queue.emplace_back(child);
      }
      if (parent->children.empty()) {
        leaves_.erase(parent->index.hash);
      }
      parent->children.clear();
      nodes_.erase(parent->index.hash);
    }
    std::ranges::reverse(changes.prune);
    root_ = new_finalized;
    root_->detach();
    if (changes.reorg) {
      forceRefreshBest();
      size_t offset = changes.reorg->apply.size();
      [[maybe_unused]] auto ok = descend(
          best_, new_finalized, [&](const std::shared_ptr<TreeNode> &node) {
            changes.reorg->apply.emplace_back(node->index);
          });
      BOOST_ASSERT(ok);
      std::reverse(
          // NOLINTNEXTLINE(cppcoreguidelines-narrowing-conversions)
          changes.reorg->apply.begin() + offset,
          changes.reorg->apply.end());
    }
    return changes;
  }

  ReorgAndPrune CachedTree::removeLeaf(const BlockHash &hash) {
    ReorgAndPrune changes;
    auto node_it = nodes_.find(hash);
    BOOST_ASSERT(node_it != nodes_.end());
    auto &node = node_it->second;
    BOOST_ASSERT(node);
    auto leaf_it = leaves_.find(hash);
    BOOST_ASSERT(leaf_it != leaves_.end());
    BOOST_ASSERT(node->children.empty());
    auto parent = node->parent();
    [[unlikely]] if (not parent) { throw std::bad_weak_ptr{}; }
    auto child_it = std::ranges::find(parent->children, node);
    BOOST_ASSERT(child_it != parent->children.end());
    changes.prune.emplace_back(node->index);
    parent->children.erase(child_it);
    if (parent->children.empty()) {
      leaves_.emplace(parent->index.hash, parent->index.slot);
    }
    leaves_.erase(leaf_it);
    if (node == best_) {
      auto old_best = node;
      forceRefreshBest();
      changes.reorg = reorg(old_best, best_);
    }
    nodes_.erase(node_it);
    return changes;
  }

  ReorgAndPrune CachedTree::removeUnfinalized() {
    ReorgAndPrune changes;
    if (best_ != root_) {
      changes.reorg = reorg(best_, root_);
    }
    std::deque<std::shared_ptr<TreeNode>> queue{root_};
    while (not queue.empty()) {
      auto parent = std::move(queue.front());
      queue.pop_front();
      for (auto &child : parent->children) {
        changes.prune.emplace_back(child->index);
        queue.emplace_back(child);
      }
      parent->children.clear();
    }
    std::ranges::reverse(changes.prune);
    *this = CachedTree{root_->index};
    return changes;
  }
}  // namespace lean::blockchain
