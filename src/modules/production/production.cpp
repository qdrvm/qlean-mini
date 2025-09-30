/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "modules/production/production.hpp"

#include <qtils/visit_in_place.hpp>

#include "blockchain/block_tree.hpp"
#include "crypto/hasher.hpp"
#include "modules/shared/networking_types.tmp.hpp"
#include "modules/shared/prodution_types.tmp.hpp"
#include "types/block_data.hpp"
#include "types/signed_block.hpp"
#include "utils/__debug_env.hpp"

namespace lean::modules {
  ProductionModuleImpl::ProductionModuleImpl(
      ProductionLoader &loader,
      qtils::SharedRef<log::LoggingSystem> logging_system,
      qtils::SharedRef<blockchain::BlockTree> block_tree,
      std::shared_ptr<ForkChoiceStore> fork_choice_store,
      qtils::SharedRef<crypto::Hasher> hasher,
      qtils::SharedRef<clock::SystemClock> clock)
      : loader_(loader),
        logsys_(std::move(logging_system)),
        logger_(logsys_->getLogger("ProductionModule", "production_module")),
        block_tree_(std::move(block_tree)),
        fork_choice_store_(std::move(fork_choice_store)),
        hasher_(std::move(hasher)),
        clock_(std::move(clock)) {}

  void ProductionModuleImpl::on_loaded_success() {
    SL_INFO(logger_, "Loaded success");
  }

  void ProductionModuleImpl::on_loading_is_finished() {
    SL_INFO(logger_, "Loading is finished");
  }

  void ProductionModuleImpl::on_slot_interval_started(
      std::shared_ptr<const messages::SlotIntervalStarted> msg) {
    // advance fork choice store to current time
    auto res = fork_choice_store_->advanceTime(clock_->nowSec());

    // dispatch all votes and blocks produced during advance time
    for (auto &vote_or_block : res) {
      qtils::visit_in_place(
          vote_or_block,
          [&](const SignedVote &v) {
            loader_.dispatchSendSignedVote(
                std::make_shared<messages::SendSignedVote>(v));
          },
          [&](const SignedBlock &v) {
            loader_.dispatchSendSignedBlock(
                std::make_shared<messages::SendSignedBlock>(v));
            auto res = block_tree_->addBlock(v.message);
            if (!res.has_value()) {
              SL_ERROR(
                  logger_, "Failed to add produced block: {}", res.error());
            }
          });
    }
  }

  void ProductionModuleImpl::on_leave_update(
      std::shared_ptr<const messages::NewLeaf> msg) {
    SL_INFO(logger_,
            "New leaf {} appeared{}",
            msg->header.index(),
            msg->best ? "; it's the new best leaf" : "");
  }

  void ProductionModuleImpl::on_block_finalized(
      std::shared_ptr<const messages::Finalized> msg) {
    SL_INFO(logger_, "Chain finalized on block {}", msg->finalized);
    for (auto retired : msg->retired) {
      SL_INFO(logger_, "Block {} is retired", retired);
    }
  }

}  // namespace lean::modules
