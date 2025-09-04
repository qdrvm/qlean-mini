/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "modules/production/production.hpp"

#include "blockchain/block_tree.hpp"
#include "crypto/hasher.hpp"
#include "modules/shared/prodution_types.tmp.hpp"
#include "types/block_data.hpp"
#include "types/signed_block.hpp"

namespace lean::modules {
  ProductionModuleImpl::ProductionModuleImpl(
      ProductionLoader &loader,
      qtils::SharedRef<log::LoggingSystem> logging_system,
      qtils::SharedRef<blockchain::BlockTree> block_tree,
      qtils::SharedRef<crypto::Hasher> hasher)
      : loader_(loader),
        logsys_(std::move(logging_system)),
        logger_(logsys_->getLogger("ProductionModule", "production_module")),
        block_tree_(std::move(block_tree)),
        hasher_(std::move(hasher)) {}

  void ProductionModuleImpl::on_loaded_success() {
    SL_INFO(logger_, "Loaded success");
  }

  void ProductionModuleImpl::on_loading_is_finished() {
    SL_INFO(logger_, "Loading is finished");
  }

  void ProductionModuleImpl::on_slot_started(
      std::shared_ptr<const messages::SlotStarted> msg) {
    if (msg->epoch_change) {
      SL_INFO(logger_, "Epoch changed to {}", msg->epoch_change);
    }

    auto is_producer = msg->slot % 3 == 2;  // qdrvm validator indices for dev

    SL_INFO(logger_,
            "Slot {} is started{}",
            msg->slot,
            is_producer ? " - I'm a producer" : "");

    if (is_producer) {
      auto parent_hash = block_tree_->bestBlock().hash;
      // Produce block
      BlockBody body;

      BlockHeader header;
      header.slot = msg->slot;
      header.proposer_index = 2;
      header.parent_root = parent_hash;
      header.state_root = {};
      header.body_root = {};
      header.updateHash(*hasher_);

      BlockData block_data;
      block_data.hash = header.hash();
      block_data.header.emplace(header);
      block_data.body.emplace(body);
      block_data.signature = {};

      Block block;
      block.slot = msg->slot;
      block.proposer_index = 2;
      block.parent_root = parent_hash;
      block.state_root = {};
      block.body = body;

      // Add a block into the block tree
      auto res = block_tree_->addBlock(block);
      if (res.has_error()) {
        SL_ERROR(
            logger_, "Could not add block to the block tree: {}", res.error());
        return;
      }

      // Notify subscribers
      loader_.dispatch_block_produced(std::make_shared<const Block>(block));
    }
  }

  void ProductionModuleImpl::on_leave_update(
      std::shared_ptr<const messages::NewLeaf> msg) {
    SL_INFO(logger_,
            "New leaf {} appeared{}",
            msg->header.index(),
            msg->best ? "; it's the new the best leaf" : "");
  }

  void ProductionModuleImpl::on_block_finalized(
      std::shared_ptr<const messages::Finalized> msg) {
    SL_INFO(logger_, "Chain finalized on block {}", msg->finalized);
    for (auto retired : msg->retired) {
      SL_INFO(logger_, "Block {} is retired", retired);
    }
  }

}  // namespace lean::modules
