/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "modules/production/production.hpp"

#include "modules/shared/prodution_types.tmp.hpp"
#include "types/signed_block.hpp"

namespace lean::modules {
  ProductionModuleImpl::ProductionModuleImpl(
      ProductionLoader &loader,
      qtils::SharedRef<log::LoggingSystem> logging_system)
      : loader_(loader),
        logsys_(std::move(logging_system)),
        logger_(logsys_->getLogger("ProductionModule", "production_module")) {}

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

    auto is_producer = msg->slot % 3 == 2;

    SL_INFO(logger_,
            "Slot {} is started{}",
            msg->slot,
            is_producer ? " - I'm a producer" : "");

    if (is_producer) {
      // Produce block
      Block block{
          .slot = msg->slot,
          .proposer_index = 2,
          .parent_root = {},
          .state_root = {},
          .body = {},
      };

      // Sign block
      SignedBlock s_block{
          .message = block,
          .signature = {},
      };

      // Add a block into the block tree
      // block_tree->addBlock(s_block);

      // Notify subscribers
      loader_.dispatch_block_produced(std::make_shared<const Block>(block));
    }
  }

  void ProductionModuleImpl::on_leave_update(
      std::shared_ptr<const messages::NewLeaf> msg) {
    SL_INFO(logger_,
            "New leaf {} appeared{}",
            msg->leaf,
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
