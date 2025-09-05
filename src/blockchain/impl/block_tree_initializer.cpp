/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */


#include "blockchain/impl/block_tree_initializer.hpp"

#include <set>
#include <unordered_set>

#include <qtils/error_throw.hpp>

#include "blockchain/block_storage.hpp"
#include "blockchain/block_storage_error.hpp"
#include "blockchain/block_tree_error.hpp"
#include "log/logger.hpp"
#include "types/justification.hpp"

namespace lean::blockchain {

  namespace {
    /// Function-helper for loading (and repair if it needed) of leaves
    outcome::result<std::set<BlockIndex>> loadLeaves(
        const qtils::SharedRef<BlockStorage> &storage, const log::Logger &log) {
      std::set<BlockIndex> block_tree_leaves;
      {
        auto block_tree_unordered_leaves_res = storage->getBlockTreeLeaves();
        if (block_tree_unordered_leaves_res.has_value()) {
          auto &block_tree_unordered_leaves =
              block_tree_unordered_leaves_res.value();
          SL_TRACE(log,
                   "List of leaves has loaded: {} leaves",
                   block_tree_unordered_leaves.size());

          for (auto &hash : block_tree_unordered_leaves) {
            // get slot of block by hash
            const auto header = storage->getBlockHeader(hash);
            if (not header) {
              if (header
                  == outcome::failure(BlockTreeError::HEADER_NOT_FOUND)) {
                SL_TRACE(log, "Leaf {} not found", hash);
                continue;
              }
              SL_ERROR(log, "Leaf {} is corrupted: {}", hash, header.error());
              return header.as_failure();
            }
            auto slot = header.value().slot;
            SL_TRACE(log, "Leaf {} found", BlockIndex(slot, hash));
            block_tree_leaves.emplace(slot, hash);
          }
        } else if (false  // No try to repair
                   or block_tree_unordered_leaves_res.error()
                          != BlockStorageError::BLOCK_TREE_LEAVES_NOT_FOUND) {
          return block_tree_unordered_leaves_res.error();
        }
      }

      if (block_tree_leaves.empty()) {
        OUTCOME_TRY(it, storage->seekLastSlot());
        if (not it.isValid()) {
          SL_WARN(log, "No one block was found");
          return BlockStorageError::NO_BLOCKS_FOUND;
        }

        SL_WARN(log, "No one leaf was found. Trying to repair");
        while (it.isValid()) {
          SL_WARN(log, "Found block at slot {}", it.slot());
          --it;
        }

        Slot slot = 0;
        auto lower = std::numeric_limits<Slot>::min();
        auto upper = std::numeric_limits<Slot>::max();

        for (;;) {
          slot = lower + (upper - lower) / 2 + 1;

          auto hashes_opt_res = storage->getBlockHash(slot);
          if (hashes_opt_res.has_failure()) {
            SL_CRITICAL(log,
                        "Search best block has failed: {}",
                        hashes_opt_res.error());
            return BlockTreeError::HEADER_NOT_FOUND;
          }
          const auto &hashes = hashes_opt_res.value();

          if (not hashes.empty()) {
            SL_TRACE(log, "bisect {} -> found", slot);
            lower = slot;
          } else {
            SL_TRACE(log, "bisect {} -> not found", slot);
            upper = slot - 1;
          }
          if (lower == upper) {
            slot = lower;
            break;
          }
        }

        OUTCOME_TRY(hashes, storage->getBlockHash(slot));
        for (auto &hash : hashes) {
          block_tree_leaves.emplace(slot, hash);
        }

        if (auto res = storage->setBlockTreeLeaves(hashes); res.has_error()) {
          SL_CRITICAL(
              log, "Can't save recovered block tree leaves: {}", res.error());
          return res.as_failure();
        }
      }

      return block_tree_leaves;
    }
  }  // namespace

  BlockTreeInitializer::BlockTreeInitializer(
      qtils::SharedRef<log::LoggingSystem> logsys,
      qtils::SharedRef<BlockStorage> storage) {
    auto logger = logsys->getLogger("BlockTree", "block_tree");

    // Load (or recalculate) leaves
    auto block_tree_leaves_res = loadLeaves(storage, logger);
    if (block_tree_leaves_res.has_error()) {
      SL_CRITICAL(logger,
                  "Failed to load block tree leaves: {}",
                  block_tree_leaves_res.error());
      qtils::raise(block_tree_leaves_res.error());
    }

    // Ensure if a list of leaves is empty
    auto &block_tree_leaves = block_tree_leaves_res.value();
    if (block_tree_leaves.empty()) {
      SL_CRITICAL(logger, "Not found any block tree leaves");
      qtils::raise(BlockTreeError::BLOCK_TREE_CORRUPTED);
    }

    // Get the last finalized block
    auto last_finalized_block_index_res = storage->getLastFinalized();
    if (last_finalized_block_index_res.has_error()) {
      SL_CRITICAL(logger,
                  "Failed to get last finalized block info: {}",
                  last_finalized_block_index_res.error());
      qtils::raise(last_finalized_block_index_res.error());
    }
    auto &last_finalized_block_index = last_finalized_block_index_res.value();

    // Get its header
    auto finalized_block_header_res =
        storage->getBlockHeader(last_finalized_block_index.hash);
    if (finalized_block_header_res.has_error()) {
      SL_CRITICAL(logger,
                  "Failed to get last finalized block header: {}",
                  finalized_block_header_res.error());
      qtils::raise(finalized_block_header_res.error());
    }
    // auto &finalized_block_header = finalized_block_header_res.value();
    //
    // // call chain_events_engine->notify to init babe_config_repo preventive
    // chain_events_engine->notify(
    //     events::ChainEventType::kFinalizedHeads,
    //     finalized_block_header);

    // // Ensure if last_finalized_block_info has the necessary justifications
    // OUTCOME_TRY(storage->getJustification(last_finalized_block_info.hash));

    // Last known block
    auto last_known_block = *block_tree_leaves.rbegin();
    SL_INFO(logger,
            "Last known block: {}, Last finalized: {}",
            last_known_block,
            last_finalized_block_index);

    // Load non-finalized block from block storage
    std::map<BlockIndex, BlockHeader> collected;

    {
      std::unordered_set<BlockHash> observed;
      std::unordered_set<BlockIndex> dead;
      // Iterate leaves
      for (auto &leaf : block_tree_leaves) {
        std::unordered_set<BlockIndex> subchain;
        // Iterate subchain from leaf to finalized or early observer
        for (auto block = leaf;;) {
          // Met last finalized
          if (block.hash == last_finalized_block_index.hash) {
            break;
          }

          // Met early observed block
          if (observed.contains(block.hash)) {
            break;
          }

          // Met known dead block
          if (dead.contains(block)) {
            dead.insert(subchain.begin(), subchain.end());
            break;
          }

          // Check if non-pruned fork has detected
          if (block.slot == last_finalized_block_index.slot) {
            dead.insert(subchain.begin(), subchain.end());

            auto main = last_finalized_block_index;
            auto fork = block;

            // Collect as the dead all blocks that differ from the finalized
            // chain
            for (;;) {
              dead.emplace(fork);

              auto f_res = storage->getBlockHeader(fork.hash);
              if (f_res.has_error()) {
                break;
              }
              const auto &fork_header = f_res.value();

              auto m_res = storage->getBlockHeader(main.hash);
              if (m_res.has_error()) {
                break;
              }
              const auto &main_header = m_res.value();

              BOOST_ASSERT(fork_header.slot == main_header.slot);
              if (fork_header.parent_root == main_header.parent_root) {
                break;
              }

              fork = {fork_header.slot, fork_header.hash()};
              main = {main_header.slot, main_header.hash()};
            }

            break;
          }

          subchain.emplace(block);

          auto header_res = storage->getBlockHeader(block.hash);
          if (header_res.has_error()) {
            SL_CRITICAL(
                logger,
                "Can't get header of existing non-finalized block {}: {}",
                block,
                header_res.error());
            qtils::raise(BlockTreeError::BLOCK_TREE_CORRUPTED);
          }

          observed.emplace(block.hash);

          auto &header = header_res.value();
          if (header.slot < last_finalized_block_index.slot) {
            SL_WARN(logger,
                    "Detected a leaf {} lower than the last finalized block {}",
                    block,
                    last_finalized_block_index);
            break;
          }

          auto [it, ok] = collected.emplace(block, std::move(header));

          block = {it->second.slot, it->second.hash()};
        }
      }

      if (not dead.empty()) {
        SL_WARN(logger,
                "Found {} orphan blocks; "
                "these block will be removed for consistency",
                dead.size());
        for (auto &block : dead) {
          collected.erase(block);
          std::ignore = storage->removeBlock(block.hash);
        }
      }
    }

    // Prepare and create a block tree basing last finalized block
    SL_DEBUG(logger, "Last finalized block {}", last_finalized_block_index);

    last_finalized_ = last_finalized_block_index;
    non_finalized_ = std::move(collected);
  }

  std::tuple<BlockIndex, std::map<BlockIndex, BlockHeader>>
  BlockTreeInitializer::nonFinalizedSubTree() {
    // if (used_.test_and_set()) {
    //   qtils::raise(BlockTreeError::WRONG_WORKFLOW);
    // }
    // return std::make_tuple(last_finalized_, std::move(non_finalized_));
    return std::make_tuple(last_finalized_, non_finalized_);
  }

}  // namespace lean::blockchain
