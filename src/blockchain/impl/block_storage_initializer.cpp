/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */


#include "blockchain/impl/block_storage_initializer.hpp"

#include <log/logger.hpp>
#include <qtils/error_throw.hpp>
#include <storage/spaced_storage.hpp>

#include "blockchain/block_storage_error.hpp"
#include "blockchain/genesis_block_header.hpp"
#include "blockchain/impl/block_storage_impl.hpp"
#include "blockchain/impl/storage_util.hpp"
#include "types/block.hpp"
#include "types/state.hpp"

namespace lean::blockchain {

  BlockStorageInitializer::BlockStorageInitializer(
      qtils::SharedRef<log::LoggingSystem> logsys,
      qtils::SharedRef<storage::SpacedStorage> storage,
      qtils::SharedRef<AnchorBlock> anchor_block,
      qtils::SharedRef<AnchorState> anchor_state,
      qtils::SharedRef<app::ChainSpec> chain_spec,
      qtils::SharedRef<crypto::Hasher> hasher) {
    // temporary instance of block storage
    BlockStorageImpl block_storage(std::move(logsys), storage, hasher, {});

    auto anchor_block_hash = anchor_block->hash();

    // Try to get genesis header from storage
    auto anchor_block_existing_res =
        block_storage.hasBlockHeader(anchor_block_hash);
    if (anchor_block_existing_res.has_error()) {
      block_storage.logger_->critical(
          "Database error at check existing genesis block: {}",
          anchor_block_existing_res.error());
      qtils::raise(anchor_block_existing_res.error());
    }
    auto anchor_header_is_exist = anchor_block_existing_res.value();

    if (not anchor_header_is_exist) {
      // anchor block initialization
      BlockData anchor_block_data;
      anchor_block_data.header.emplace(anchor_block->getHeader());
      anchor_block_data.body.emplace(anchor_block->body);
      anchor_block_data.hash = anchor_block->hash();
      BOOST_ASSERT(anchor_block_hash == anchor_block_data.hash);

      // Store anchor block data
      auto res = block_storage.putBlock(anchor_block_data);
      if (res.has_error()) {
        block_storage.logger_->critical(
            "Database error at store genesis block into: {}", res.error());
        qtils::raise(res.error());
      }
      BOOST_ASSERT(anchor_block_hash == res.value());

      // Assign block's hash and slot
      auto assignment_res =
          block_storage.assignHashToSlot(anchor_block->index());
      if (assignment_res.has_error()) {
        block_storage.logger_->critical(
            "Database error at assigning genesis block hash: {}",
            assignment_res.error());
        qtils::raise(assignment_res.error());
      }

      // Initial set of leaves
      auto sel_leaves_res =
          block_storage.setBlockTreeLeaves({anchor_block->hash()});
      if (sel_leaves_res.has_error()) {
        block_storage.logger_->critical(
            "Database error at set genesis block as leaf: {}",
            sel_leaves_res.error());
        qtils::raise(sel_leaves_res.error());
      }

      // Store anchor state
      auto state_res =
          block_storage.putState(anchor_block->hash(), *anchor_state);
      if (state_res.has_error()) {
        block_storage.logger_->critical(
            "Database error at store anchor state into: {}", res.error());
        qtils::raise(state_res.error());
      }

      block_storage.logger_->info("Anchor block {}, state {}",
                                  anchor_block->index(),
                                  anchor_block->state_root);
    }
  }

}  // namespace lean::blockchain
