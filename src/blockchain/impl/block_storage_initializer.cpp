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

namespace lean::blockchain {

  BlockStorageInitializer::BlockStorageInitializer(
      qtils::SharedRef<log::LoggingSystem> logsys,
      qtils::SharedRef<storage::SpacedStorage> storage,
      qtils::SharedRef<GenesisBlockHeader> genesis_header,
      qtils::SharedRef<app::ChainSpec> chain_spec,
      qtils::SharedRef<crypto::Hasher> hasher) {
    // temporary instance of block storage
    BlockStorageImpl block_storage(std::move(logsys), storage, hasher, {});

    auto genesis_block_hash = genesis_header->hash();

    // Try to get genesis header from storage
    auto genesis_block_existing_res =
        block_storage.hasBlockHeader(genesis_block_hash);
    if (genesis_block_existing_res.has_error()) {
      block_storage.logger_->critical(
          "Database error at check existing genesis block: {}",
          genesis_block_existing_res.error());
      qtils::raise(genesis_block_existing_res.error());
    }
    auto genesis_header_is_exist = genesis_block_existing_res.value();

    if (not genesis_header_is_exist) {
      // genesis block initialization
      BlockData genesis_block;
      genesis_block.header.emplace(*genesis_header);

      auto res = block_storage.putBlock(genesis_block);
      if (res.has_error()) {
        block_storage.logger_->critical(
            "Database error at store genesis block into: {}", res.error());
        qtils::raise(res.error());
      }
      BOOST_ASSERT(genesis_block_hash == res.value());

      auto assignment_res =
          block_storage.assignHashToSlot(genesis_header->index());
      if (assignment_res.has_error()) {
        block_storage.logger_->critical(
            "Database error at assigning genesis block hash: {}",
            assignment_res.error());
        qtils::raise(assignment_res.error());
      }

      auto sel_leaves_res =
          block_storage.setBlockTreeLeaves({genesis_header->hash()});
      if (sel_leaves_res.has_error()) {
        block_storage.logger_->critical(
            "Database error at set genesis block as leaf: {}",
            sel_leaves_res.error());
        qtils::raise(sel_leaves_res.error());
      }

      // TODO Save genesis state here

      block_storage.logger_->info("Genesis block {}, state {}",
                                  genesis_block_hash,
                                  genesis_header->state_root);
    }
  }

}  // namespace lean::blockchain
