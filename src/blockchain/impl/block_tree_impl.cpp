/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/impl/block_tree_impl.hpp"

#include <queue>
#include <stack>

#include <qtils/cxx23/ranges/contains.hpp>

#include "blockchain/block_tree_error.hpp"
#include "blockchain/impl/block_tree_initializer.hpp"
#include "blockchain/impl/cached_tree.hpp"
#include "modules/shared/prodution_types.tmp.hpp"
#include "se/subscription.hpp"
#include "se/subscription_fwd.hpp"
#include "types/block.hpp"
#include "types/block_header.hpp"
#include "types/signed_block_with_attestation.hpp"

namespace lean::blockchain {
  BlockTreeImpl::SafeBlockTreeData::SafeBlockTreeData(BlockTreeData data)
      : block_tree_data_{std::move(data)} {}

  BlockTreeImpl::BlockTreeImpl(
      qtils::SharedRef<log::LoggingSystem> logsys,
      qtils::SharedRef<BlockStorage> storage,
      qtils::SharedRef<crypto::Hasher> hasher,
      std::shared_ptr<Subscription> se_manager,
      qtils::SharedRef<BlockTreeInitializer> initializer)
      : log_(logsys->getLogger("BlockTree", "block_tree")),
        se_manager_(std::move(se_manager)),
        block_tree_data_{{
            .storage_ = std::move(storage),
            .tree_ = std::make_unique<CachedTree>(
                std::get<0>(initializer->nonFinalizedSubTree())),
            .hasher_ = std::move(hasher),
        }} {
    // Add non-finalized block to the block tree
    for (const auto &[block, header] :
         std::get<1>(initializer->nonFinalizedSubTree())) {
      auto res = BlockTreeImpl::addExistingBlock(block.hash, header);
      if (res.has_error()) {
        SL_WARN(
            log_, "Failed to add existing block {}: {}", block, res.error());
      }
      SL_TRACE(log_,
               "Existing non-finalized block {} is added to block tree",
               block);
    }
  }

  const BlockHash &BlockTreeImpl::getGenesisBlockHash() const {
    return block_tree_data_
        .sharedAccess([&](const BlockTreeData &p)
                          -> std::reference_wrapper<const BlockHash> {
          if (p.genesis_block_hash_.has_value()) {
            return p.genesis_block_hash_.value();
          }

          auto res = p.storage_->getBlockHash(0);
          BOOST_ASSERT_MSG(res.has_value() and res.value().size() == 1,
                           "Block tree must contain exactly one genesis block");

          // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
          const_cast<std::optional<BlockHash> &>(p.genesis_block_hash_)
              .emplace(res.value()[0]);
          return p.genesis_block_hash_.value();
        })
        .get();
  }

  outcome::result<void> BlockTreeImpl::addBlockHeader(
      const BlockHeader &header) {
    return block_tree_data_.exclusiveAccess(
        [&](BlockTreeData &p) -> outcome::result<void> {
          auto parent_opt = p.tree_->find(header.parent_root);
          if (not parent_opt.has_value()) {
            return BlockTreeError::NO_PARENT;
          }
          const auto &parent = parent_opt.value();
          OUTCOME_TRY(p.storage_->putBlockHeader(header));

          // update local meta with the new block
          auto new_node = std::make_shared<TreeNode>(header.index(), parent);

          auto reorg = p.tree_->add(new_node);
          OUTCOME_TRY(reorgAndPrune(p, {std::move(reorg), {}}));

          auto header_ptr = std::make_shared<BlockHeader>(header);
          se_manager_->notify(EventTypes::BlockAdded, header_ptr);

          SL_VERBOSE(
              log_, "Block {} has been added into block tree", header.index());

          return outcome::success();
        });
  }

  outcome::result<void> BlockTreeImpl::addBlock(
      SignedBlockWithAttestation signed_block_with_attestation) {
    auto &block = signed_block_with_attestation.message.block;
    return block_tree_data_.exclusiveAccess(
        [&](BlockTreeData &p) -> outcome::result<void> {
          // Check if we know parent of this block; if not, we cannot insert it
          auto parent_opt = p.tree_->find(block.parent_root);
          if (not parent_opt.has_value()) {
            return BlockTreeError::NO_PARENT;
          }
          const auto &parent = parent_opt.value();

          auto header = block.getHeader();
          header.updateHash();

          SL_DEBUG(log_, "Adding block {}", header.index());

          BlockData block_data;
          block_data.hash = header.hash();
          block_data.header.emplace(header);
          block_data.body.emplace(block.body);
          block_data.signature = {};

          // Save block
          OUTCOME_TRY(block_hash, p.storage_->putBlock(block_data));
          BOOST_ASSERT(block_hash == header.hash());

          // Update local meta with the block
          auto new_node = std::make_shared<TreeNode>(header.index(), parent);

          auto reorg = p.tree_->add(new_node);
          OUTCOME_TRY(reorgAndPrune(p, {std::move(reorg), {}}));

          auto msg = std::make_shared<const messages::NewLeaf>(
              header, header.index() == bestBlock());
          se_manager_->notify(EventTypes::BlockAdded, msg);

          SL_VERBOSE(
              log_, "Block {} has been added into block tree", header.index());
          return outcome::success();
        });
  }

  outcome::result<void> BlockTreeImpl::removeLeaf(const BlockHash &block_hash) {
    return block_tree_data_.exclusiveAccess(
        [&](BlockTreeData &p) -> outcome::result<void> {
          auto finalized = getLastFinalizedNoLock(p);
          if (block_hash == finalized.hash) {
            OUTCOME_TRY(header, getBlockHeader(block_hash));
            // OUTCOME_TRY(p.storage_->removeJustification(finalized.hash));

            OUTCOME_TRY(slot, getSlotByHash(header.parent_root));
            auto parent = BlockIndex(slot, header.parent_root);

            ReorgAndPrune changes{
                .reorg = Reorg{.common = parent, .revert = {finalized}},
                .prune = {finalized},
            };
            p.tree_ = std::make_unique<CachedTree>(parent);
            OUTCOME_TRY(reorgAndPrune(p, changes));
            return outcome::success();
          }
          if (not p.tree_->isLeaf(block_hash)) {
            return BlockTreeError::BLOCK_IS_NOT_LEAF;
          }
          auto changes = p.tree_->removeLeaf(block_hash);
          OUTCOME_TRY(reorgAndPrune(p, changes));
          return outcome::success();
        });
  }

  // outcome::result<void> BlockTreeImpl::markAsParachainDataBlock(
  //     const BlockHash &block_hash) {
  //   return block_tree_data_.exclusiveAccess(
  //       [&](BlockTreeData &p) -> outcome::result<void> {
  //         SL_TRACE(log_, "Trying to adjust weight for block {}", block_hash);
  //
  //         auto node = p.tree_->find(block_hash);
  //         if (node == nullptr) {
  //           SL_WARN(log_, "Block {} doesn't exists in block tree",
  //           block_hash); return BlockTreeError::BLOCK_NOT_EXISTS;
  //         }
  //
  //         node->contains_approved_para_block = true;
  //         return outcome::success();
  //       });
  // }

  // outcome::result<void> BlockTreeImpl::markAsRevertedBlocks(
  //     const std::vector<BlockHash> &block_hashes) {
  //   return block_tree_data_.exclusiveAccess(
  //       [&](BlockTreeData &p) -> outcome::result<void> {
  //         bool need_to_refresh_best = false;
  //         auto best = bestBlockNoLock(p);
  //         for (const auto &block_hash : block_hashes) {
  //           auto node_opt = p.tree_->find(block_hash);
  //           if (not node_opt.has_value()) {
  //             SL_WARN(
  //                 log_, "Block {} doesn't exists in block tree", block_hash);
  //             continue;
  //           }
  //           auto &node = node_opt.value();
  //
  //           if (not node->reverted) {
  //             std::queue<std::shared_ptr<TreeNode>> to_revert;
  //             to_revert.push(std::move(node));
  //             while (not to_revert.empty()) {
  //               auto &reverting_tree_node = to_revert.front();
  //
  //               reverting_tree_node->reverted = true;
  //
  //               if (reverting_tree_node->info == best) {
  //                 need_to_refresh_best = true;
  //               }
  //
  //               for (auto &child : reverting_tree_node->children) {
  //                 if (not child->reverted) {
  //                   to_revert.push(child);
  //                 }
  //               }
  //
  //               to_revert.pop();
  //             }
  //           }
  //         }
  //         if (need_to_refresh_best) {
  //           p.tree_->forceRefreshBest();
  //         }
  //         return outcome::success();
  //       });
  // }

  outcome::result<void> BlockTreeImpl::addExistingBlockNoLock(
      BlockTreeData &p,
      const BlockHash &block_hash,
      const BlockHeader &block_header) {
    SL_TRACE(log_,
             "Trying to add block {} into block tree",
             BlockIndex(block_header.slot, block_hash));

    auto node_opt = p.tree_->find(block_hash);
    // Check if the tree doesn't have this block; if not, we skip that
    if (node_opt.has_value()) {
      SL_TRACE(log_,
               "Block {} exists in block tree",
               BlockIndex(block_header.slot, block_hash));
      return BlockTreeError::BLOCK_EXISTS;
    }

    auto parent_opt = p.tree_->find(block_header.parent_root);

    // Check if we know the parent of this block; if not, we cannot insert it
    if (not parent_opt.has_value()) {
      SL_TRACE(log_,
               "Block {} parent of {} has not found in block tree. "
               "Trying to restore missed branch",
               block_header.parent_root,
               BlockIndex(block_header.slot, block_hash));

      // Trying to restore a missed branch
      std::stack<std::pair<BlockHash, BlockHeader>> to_add;

      auto finalized = getLastFinalizedNoLock(p).slot;

      for (auto hash = block_header.parent_root;;) {
        OUTCOME_TRY(header, p.storage_->getBlockHeader(hash));
        BlockIndex block_index(header.slot, hash);
        SL_TRACE(log_,
                 "Block {} has found in storage and enqueued to add",
                 block_index);

        if (header.slot <= finalized) {
          return BlockTreeError::BLOCK_ON_DEAD_END;
        }

        auto parent_hash = header.parent_root;
        to_add.emplace(hash, header);

        if (p.tree_->find(parent_hash).has_value()) {
          SL_TRACE(log_,
                   "Block {} parent of {} has found in block tree",
                   parent_hash,
                   block_index);
          break;
        }

        SL_TRACE(log_,
                 "Block {} has not found in block tree. "
                 "Trying to restore from storage",
                 parent_hash);

        hash = parent_hash;
      }

      while (not to_add.empty()) {
        const auto &[hash, header] = to_add.top();
        OUTCOME_TRY(addExistingBlockNoLock(p, hash, header));
        to_add.pop();
      }

      parent_opt = p.tree_->find(block_header.parent_root);
      BOOST_ASSERT_MSG(parent_opt.has_value(),
                       "Parent must be restored at this moment");

      SL_TRACE(log_,
               "Trying to add block {} into block tree",
               BlockIndex(block_header.slot, block_hash));
    }
    auto &parent = parent_opt.value();

    // Update local meta with the block
    auto new_node = std::make_shared<TreeNode>(block_header.index(), parent);

    auto reorg = p.tree_->add(new_node);
    OUTCOME_TRY(reorgAndPrune(p, {std::move(reorg), {}}));

    SL_VERBOSE(log_,
               "Block {} has been restored in block tree from storage",
               block_header.index());

    return outcome::success();
  }

  outcome::result<void> BlockTreeImpl::addExistingBlock(
      const BlockHash &block_hash, const BlockHeader &block_header) {
    return block_tree_data_.exclusiveAccess(
        [&](BlockTreeData &p) -> outcome::result<void> {
          return addExistingBlockNoLock(p, block_hash, block_header);
        });
  }

  outcome::result<void> BlockTreeImpl::addBlockBody(const BlockHash &block_hash,
                                                    const BlockBody &body) {
    return block_tree_data_.exclusiveAccess(
        [&](const BlockTreeData &p) -> outcome::result<void> {
          return p.storage_->putBlockBody(block_hash, body);
        });
  }

  outcome::result<void> BlockTreeImpl::finalize(const BlockHash &block_hash) {
    return block_tree_data_.exclusiveAccess(
        [&](BlockTreeData &p) -> outcome::result<void> {
          auto last_finalized_block_info = getLastFinalizedNoLock(p);
          if (block_hash == last_finalized_block_info.hash) {
            return outcome::success();
          }
          const auto node_opt = p.tree_->find(block_hash);
          if (node_opt.has_value()) {
            auto &node = node_opt.value();

            SL_DEBUG(log_, "Finalizing block {}", node->index);

            OUTCOME_TRY(header, p.storage_->getBlockHeader(block_hash));

            // Block which is finalized as ancestors of last finalized
            std::vector<BlockIndex> retired_blocks;
            for (auto parent = node->parent(); parent;
                 parent = parent->parent()) {
              retired_blocks.emplace_back(parent->index);
            }

            auto changes = p.tree_->finalize(node);
            OUTCOME_TRY(reorgAndPrune(p, changes));

            auto msg = std::make_shared<messages::Finalized>(
                header.index(), std::move(retired_blocks));
            se_manager_->notify(EventTypes::BlockFinalized, msg);

            log_->info("Finalized block {}", node->index);

          } else {
            OUTCOME_TRY(header, p.storage_->getBlockHeader(block_hash));
            const auto slot = header.slot;
            if (slot >= last_finalized_block_info.slot) {
              return BlockTreeError::NON_FINALIZED_BLOCK_NOT_FOUND;
            }

            OUTCOME_TRY(hashes, p.storage_->getBlockHash(slot));

            if (not qtils::cxx23::ranges::contains(hashes, block_hash)) {
              return BlockTreeError::BLOCK_ON_DEAD_END;
            }
          }
          return outcome::success();
        });
  }

  bool BlockTreeImpl::has(const BlockHash &hash) const {
    return block_tree_data_.sharedAccess([&](const BlockTreeData &p) {
      return p.tree_->find(hash) or p.storage_->hasBlockHeader(hash).value();
    });
  }

  outcome::result<BlockHeader> BlockTreeImpl::getBlockHeaderNoLock(
      const BlockTreeData &p, const BlockHash &block_hash) const {
    return p.storage_->getBlockHeader(block_hash);
  }

  outcome::result<BlockHeader> BlockTreeImpl::getBlockHeader(
      const BlockHash &block_hash) const {
    return block_tree_data_.sharedAccess(
        [&](const BlockTreeData &p) -> outcome::result<BlockHeader> {
          return getBlockHeaderNoLock(p, block_hash);
        });
  }

  outcome::result<std::optional<BlockHeader>> BlockTreeImpl::tryGetBlockHeader(
      const BlockHash &block_hash) const {
    return block_tree_data_.sharedAccess(
        [&](const BlockTreeData &p)
            -> outcome::result<std::optional<BlockHeader>> {
          auto header = p.storage_->getBlockHeader(block_hash);
          if (header) {
            return header.value();
          }
          const auto &header_error = header.error();
          if (header_error == BlockTreeError::HEADER_NOT_FOUND) {
            return std::nullopt;
          }
          return header_error;
        });
  }

  outcome::result<BlockBody> BlockTreeImpl::getBlockBody(
      const BlockHash &block_hash) const {
    return block_tree_data_.sharedAccess(
        [&](const BlockTreeData &p) -> outcome::result<BlockBody> {
          OUTCOME_TRY(body, p.storage_->getBlockBody(block_hash));
          if (body.has_value()) {
            return body.value();
          }
          return BlockTreeError::BODY_NOT_FOUND;
        });
  }

  // outcome::result<primitives::Justification>
  // BlockTreeImpl::getBlockJustification(
  //     const BlockHash &block_hash) const {
  //   return block_tree_data_.sharedAccess(
  //       [&](const BlockTreeData &p)
  //           -> outcome::result<primitives::Justification> {
  //         OUTCOME_TRY(justification,
  //         p.storage_->getJustification(block_hash)); if
  //         (justification.has_value()) {
  //           return justification.value();
  //         }
  //         return BlockTreeError::JUSTIFICATION_NOT_FOUND;
  //       });
  // }

  outcome::result<std::vector<BlockHash>> BlockTreeImpl::getBestChainFromBlock(
      const BlockHash &block, uint64_t maximum) const {
    return block_tree_data_.sharedAccess(
        [&](const BlockTreeData &p) -> outcome::result<std::vector<BlockHash>> {
          auto block_header_res = p.storage_->getBlockHeader(block);
          if (block_header_res.has_error()) {
            log_->error("cannot retrieve block {}: {}",
                        block,
                        block_header_res.error());
            return BlockTreeError::HEADER_NOT_FOUND;
          }
          auto start_block_number = block_header_res.value().slot;

          if (maximum == 1) {
            return std::vector{block};
          }

          auto current_depth = bestBlockNoLock(p).slot;

          if (start_block_number >= current_depth) {
            return std::vector{block};
          }

          auto count = std::min<uint64_t>(
              current_depth - start_block_number + 1, maximum);

          Slot finish_block_number = start_block_number + count - 1;

          auto finish_block_hash_res =
              p.storage_->getBlockHash(finish_block_number);
          if (finish_block_hash_res.has_error()) {
            log_->error("cannot retrieve block with number {}: {}",
                        finish_block_number,
                        finish_block_hash_res.error());
            return BlockTreeError::HEADER_NOT_FOUND;
          }
          const auto &finish_block_hash = finish_block_hash_res.value()[0];

          OUTCOME_TRY(
              chain,
              getDescendingChainToBlockNoLock(p, finish_block_hash, count));

          if (chain.back() != block) {
            return std::vector{block};
          }
          std::ranges::reverse(chain);
          return chain;
        });
  }

  outcome::result<std::vector<BlockHash>>
  BlockTreeImpl::getDescendingChainToBlockNoLock(const BlockTreeData &p,
                                                 const BlockHash &to_block,
                                                 uint64_t maximum) const {
    std::vector<BlockHash> chain;

    auto hash = to_block;

    // Try to retrieve from cached tree
    if (auto node_opt = p.tree_->find(hash); node_opt.has_value()) {
      auto node = node_opt.value();
      while (maximum > chain.size()) {
        auto parent = node->parent();
        if (not parent) {
          hash = node->index.hash;
          break;
        }
        chain.emplace_back(node->index.hash);
        node = parent;
      }
    }

    while (maximum > chain.size()) {
      auto header_res = p.storage_->getBlockHeader(hash);
      if (header_res.has_error()) {
        if (chain.empty()) {
          log_->error("Cannot retrieve block with hash {}: {}",
                      hash,
                      header_res.error());
          return header_res.error();
        }
        break;
      }
      const auto &header = header_res.value();
      chain.emplace_back(hash);

      if (header.slot == 0) {
        break;
      }
      hash = header.parent_root;
    }
    return chain;
  }

  outcome::result<std::vector<BlockHash>>
  BlockTreeImpl::getDescendingChainToBlock(const BlockHash &block,
                                           uint64_t maximum) const {
    return block_tree_data_.sharedAccess([&](const BlockTreeData &p) {
      return getDescendingChainToBlockNoLock(p, block, maximum);
    });
  }

  // outcome::result<std::vector<BlockHash>> BlockTreeImpl::getChainByBlocks(
  //     const BlockHash &ancestor, const BlockHash &descendant) const {
  //   return block_tree_data_.sharedAccess(
  //       [&](const BlockTreeData &p)
  //           -> outcome::result<std::vector<BlockHash>> {
  //         OUTCOME_TRY(from_header, p.storage_->getBlockHeader(ancestor));
  //         auto from = from_header.slot;
  //         OUTCOME_TRY(to_header, p.storage_->getBlockHeader(descendant));
  //         auto to = to_header.slot;
  //         if (to < from) {
  //           return BlockTreeError::TARGET_IS_PAST_MAX;
  //         }
  //         auto count = to - from + 1;
  //         OUTCOME_TRY(chain,
  //                     getDescendingChainToBlockNoLock(p, descendant, count));
  //         if (chain.size() != count) {
  //           return BlockTreeError::EXISTING_BLOCK_NOT_FOUND;
  //         }
  //         if (chain.back() != ancestor) {
  //           return BlockTreeError::BLOCK_ON_DEAD_END;
  //         }
  //         std::ranges::reverse(chain);
  //         return chain;
  //       });
  // }

  // bool BlockTreeImpl::hasDirectChainNoLock(
  //     const BlockTreeData &p,
  //     const BlockHash &ancestor,
  //     const BlockHash &descendant) const {
  //   if (ancestor == descendant) {
  //     return true;
  //   }
  //   auto ancestor_node_ptr = p.tree_->find(ancestor);
  //   auto descendant_node_ptr = p.tree_->find(descendant);
  //   if (ancestor_node_ptr and descendant_node_ptr) {
  //     return canDescend(*descendant_node_ptr, *ancestor_node_ptr);
  //   }
  //
  //   /*
  //    * check that ancestor is above descendant
  //    * optimization that prevents reading blockDB up the genesis
  //    * TODO (xDimon) it could be not right place for this check
  //    *  or changing logic may make it obsolete
  //    *  block numbers may be obtained somewhere else
  //    */
  //   Slot ancestor_depth = 0u;
  //   Slot descendant_depth = 0u;
  //   if (ancestor_node_ptr) {
  //     ancestor_depth = (*ancestor_node_ptr)->index.slot;
  //   } else {
  //     auto header_res = p.storage_->getBlockHeader(ancestor);
  //     if (!header_res) {
  //       return false;
  //     }
  //     ancestor_depth = header_res.value().slot;
  //   }
  //   if (descendant_node_ptr) {
  //     descendant_depth = (*descendant_node_ptr)->index.slot;
  //   } else {
  //     auto header_res = p.storage_->getBlockHeader(descendant);
  //     if (!header_res) {
  //       return false;
  //     }
  //     descendant_depth = header_res.value().slot;
  //   }
  //   if (descendant_depth < ancestor_depth) {
  //     SL_DEBUG(log_,
  //              "Ancestor block is lower. {} in comparison with {}",
  //              BlockIndex(ancestor_depth, ancestor),
  //              BlockIndex(descendant_depth, descendant));
  //     return false;
  //   }
  //
  //   // Try to use optimal way, if ancestor and descendant in the finalized
  //   // chain
  //   auto finalized = [&](const BlockHash &hash,
  //                        Slot number) {
  //     return number <= getLastFinalizedNoLock(p).slot
  //        and p.storage_->getBlockHash(number)
  //                == outcome::success(
  //                    std::optional<BlockHash>(hash));
  //   };
  //   if (descendant_node_ptr or finalized(descendant, descendant_depth)) {
  //     return finalized(ancestor, ancestor_depth);
  //   }
  //
  //   auto current_hash = descendant;
  //   while (current_hash != ancestor) {
  //     auto current_header_res = p.storage_->getBlockHeader(current_hash);
  //     if (!current_header_res) {
  //       return false;
  //     }
  //     if (current_header_res.value().slot <= ancestor_depth) {
  //       return false;
  //     }
  //     current_hash = current_header_res.value().parent_root;
  //   }
  //   return true;
  // }
  //
  // bool BlockTreeImpl::hasDirectChain(
  //     const BlockHash &ancestor,
  //     const BlockHash &descendant) const {
  //   return block_tree_data_.sharedAccess([&](const BlockTreeData &p) {
  //     return hasDirectChainNoLock(p, ancestor, descendant);
  //   });
  // }

  bool BlockTreeImpl::isFinalized(const BlockIndex &block) const {
    return block_tree_data_.sharedAccess([&](const BlockTreeData &p) {
      if (block.slot > getLastFinalizedNoLock(p).slot) {
        return false;
      }
      auto res = p.storage_->getBlockHash(block.slot);
      return res.has_value() and not res.value().empty()
         and res.value().front() == block.hash;
    });
  }

  BlockIndex BlockTreeImpl::bestBlockNoLock(const BlockTreeData &p) const {
    return p.tree_->best();
  }

  BlockIndex BlockTreeImpl::bestBlock() const {
    return block_tree_data_.sharedAccess(
        [&](const BlockTreeData &p) { return bestBlockNoLock(p); });
  }

  outcome::result<BlockIndex> BlockTreeImpl::getBestContaining(
      const BlockHash &target_hash) const {
    return block_tree_data_.sharedAccess(
        [&](const BlockTreeData &p) -> outcome::result<BlockIndex> {
          if (getLastFinalizedNoLock(p).hash == target_hash) {
            return bestBlockNoLock(p);
          }

          auto target_node_opt = p.tree_->find(target_hash);

          // If a target has not found in the block tree (in memory),
          // it means block finalized or discarded
          if (not target_node_opt.has_value()) {
            OUTCOME_TRY(target_header, p.storage_->getBlockHeader(target_hash));
            auto target_number = target_header.slot;

            OUTCOME_TRY(hashes, p.storage_->getBlockHash(target_number));

            if (not qtils::cxx23::ranges::contains(hashes, target_hash)) {
              return BlockTreeError::BLOCK_ON_DEAD_END;
            }

            return bestBlockNoLock(p);
          }

          return p.tree_->bestWith(target_node_opt.value());
        });
  }

  std::vector<BlockHash> BlockTreeImpl::getLeavesNoLock(
      const BlockTreeData &p) const {
    return p.tree_->leafHashes();
  }

  std::vector<BlockHash> BlockTreeImpl::getLeaves() const {
    return block_tree_data_.sharedAccess(
        [&](const BlockTreeData &p) { return getLeavesNoLock(p); });
  }

  // std::vector<BlockIndex> BlockTreeImpl::getLeavesInfo() const {
  //   return block_tree_data_.sharedAccess(
  //       [&](const BlockTreeData &p) { return p.tree_->leafInfo(); });
  // }

  outcome::result<std::vector<BlockHash>> BlockTreeImpl::getChildren(
      const BlockHash &block) const {
    return block_tree_data_.sharedAccess(
        [&](const BlockTreeData &p) -> outcome::result<std::vector<BlockHash>> {
          if (auto node_opt = p.tree_->find(block); node_opt.has_value()) {
            const auto &node = node_opt.value();
            std::vector<BlockHash> result;
            result.reserve(node->children.size());
            for (const auto &child : node->children) {
              result.push_back(child->index.hash);
            }
            return result;
          }
          OUTCOME_TRY(header, p.storage_->getBlockHeader(block));

          // TODO slot of children may be greater then parent's by more then one
          return p.storage_->getBlockHash(header.slot + 1);
        });
  }

  BlockIndex BlockTreeImpl::getLastFinalizedNoLock(
      const BlockTreeData &p) const {
    return p.tree_->finalized();
  }

  BlockIndex BlockTreeImpl::lastFinalized() const {
    return block_tree_data_.sharedAccess(
        [&](const BlockTreeData &p) { return getLastFinalizedNoLock(p); });
  }

  Checkpoint BlockTreeImpl::getLatestJustified() const {
    return block_tree_data_.sharedAccess([&](const BlockTreeData &p) {
      auto finalized = getLastFinalizedNoLock(p);
      // For now, return finalized as justified since we don't track separate
      // justification in basic BlockTreeImpl yet
      return Checkpoint{.root = finalized.hash, .slot = finalized.slot};
    });
  }

  outcome::result<std::optional<SignedBlockWithAttestation>>
  BlockTreeImpl::tryGetSignedBlock(const BlockHash block_hash) const {
    auto header_res = getBlockHeader(block_hash);
    if (not header_res.has_value()) {
      return std::nullopt;
    }
    auto &header = header_res.value();
    auto body_res = getBlockBody(block_hash);
    if (not body_res.has_value()) {
      return std::nullopt;
    }
    auto &body = body_res.value();
    return SignedBlockWithAttestation{
        .message =
            {
                .block =
                    {
                        .slot = header.slot,
                        .proposer_index = header.proposer_index,
                        .parent_root = header.parent_root,
                        .state_root = header.state_root,
                        .body = std::move(body),
                    },
                .proposer_attestation = {},
            },
        // TODO(turuslan): signature
        .signature = {},
    };
  }

  outcome::result<void> BlockTreeImpl::reorgAndPrune(
      const BlockTreeData &p, const ReorgAndPrune &changes) {
    OUTCOME_TRY(p.storage_->setBlockTreeLeaves(p.tree_->leafHashes()));
    if (changes.reorg) {
      for (auto &block : changes.reorg->revert) {
        OUTCOME_TRY(p.storage_->deassignHashToSlot(block));
      }
      for (auto &block : changes.reorg->apply) {
        OUTCOME_TRY(p.storage_->assignHashToSlot(block));
      }
    }

    // remove from storage
    for (const auto &[_, hash] : changes.prune) {
      OUTCOME_TRY(p.storage_->removeBlock(hash));
    }

    return outcome::success();
  }

  // BlockHeaderRepository methods

  outcome::result<Slot> BlockTreeImpl::getSlotByHash(
      const BlockHash &block_hash) const {
    auto slot_opt = block_tree_data_.sharedAccess(
        [&](const BlockTreeData &p) -> std::optional<Slot> {
          if (auto node = p.tree_->find(block_hash)) {
            return node.value()->index.slot;
          }
          return std::nullopt;
        });
    if (slot_opt.has_value()) {
      return slot_opt.value();
    }
    OUTCOME_TRY(header, getBlockHeader(block_hash));
    return header.slot;
  }

  // outcome::result<BlockHash> BlockTreeImpl::getHashByNumber(
  //     Slot number) const {
  //   OUTCOME_TRY(block_hash_opt, getBlockHash(number));
  //   if (block_hash_opt.has_value()) {
  //     return block_hash_opt.value();
  //   }
  //   return BlockTreeError::HEADER_NOT_FOUND;
  // }

}  // namespace lean::blockchain
