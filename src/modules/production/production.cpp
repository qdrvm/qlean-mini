/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "modules/production/production.hpp"

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
      qtils::SharedRef<crypto::Hasher> hasher)
      : loader_(loader),
        logsys_(std::move(logging_system)),
        logger_(logsys_->getLogger("ProductionModule", "production_module")),
        block_tree_(std::move(block_tree)),
        fork_choice_store_(std::move(fork_choice_store)),
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
      SL_INFO(logger_, "Epoch changed to {}", msg->epoch);
    }

    auto producer_index =
        msg->slot % fork_choice_store_->config_.num_validators;
    auto is_producer = getPeerIndex() == producer_index;

    SL_INFO(logger_,
            "Slot {} is started{}",
            msg->slot,
            is_producer ? " - I'm a producer" : "");

    if (is_producer) {
      fork_choice_store_->acceptNewVotes();
      SL_INFO(logger_, "States size {}", fork_choice_store_->states_.size());
      auto parent_hash = fork_choice_store_->getHead();
      auto parent_state = fork_choice_store_->getState(parent_hash);
      // Produce block
      Block block;
      block.slot = msg->slot;
      block.proposer_index = producer_index;
      block.parent_root = parent_hash;

      for (auto [validator_id, signed_vote] :
           fork_choice_store_->signed_votes_) {
        block.body.attestations.push_back(
            signed_vote);  // TODO: add cleaning of old attestations
      }

      // create signed block with signature containing only zero bytes for now
      SignedBlock signed_block{.message = block,
                               .signature = qtils::ByteArr<32>{0}};
      // calculate new state
      State new_state = fork_choice_store_->stf_
                            .stateTransition(signed_block, parent_state, false)
                            .value();
      signed_block.message.state_root = sszHash(new_state);

      // Add a block into the block tree
      // auto res = block_tree_->addBlock(block);
      // if (res.has_error()) {
      //   SL_ERROR(
      //       logger_, "Could not add block to the block tree: {}",
      //       res.error());
      //   return;
      // }

      auto on_block_res = fork_choice_store_->onBlock(signed_block.message);
      BOOST_ASSERT_MSG(on_block_res.has_value(),
                       "Fork choice store should accept produced block");

      // Notify subscribers
      // loader_.dispatch_block_produced(
      //     std::make_shared<const Block>(signed_block.message));

      loader_.dispatchSendSignedBlock(
          std::make_shared<messages::SendSignedBlock>(signed_block));
    }
  }
  void ProductionModuleImpl::on_slot_interval_one_started(
      std::shared_ptr<const messages::SlotIntervalOneStarted> msg) {
    SL_INFO(logger_, "Slot interval one started on slot {}", msg->slot);
    Checkpoint head{.root = fork_choice_store_->getHead(), .slot = msg->slot};
    auto target = fork_choice_store_->getVoteTarget();
    auto source = fork_choice_store_->getLatestJustified();
    SL_INFO(logger_,
            "For slot {}: head is {}@{}, target is {}@{}, source is {}@{}",
            msg->slot,
            head.slot,
            head.root,
            target.slot,
            target.root,
            source->slot,
            source->root);
    SignedVote signed_vote{
        .data =
            Vote{
                .validator_id = getPeerIndex(),
                .head = head,
                .target = target,
                .source = *source,
            },
        .signature = qtils::ByteArr<32>{0}  // signature with zero bytes for now
    };

    // Dispatching send signed vote only broadcasts to other peers. Current peer
    // should process attestation directly
    auto res = fork_choice_store_->processAttestation(signed_vote, false);
    BOOST_ASSERT_MSG(res.has_value(), "Produced vote should be valid");
    SL_INFO(logger_,
            "Produced vote for target {}@{}",
            signed_vote.data.target.slot,
            signed_vote.data.target.root);
    loader_.dispatchSendSignedVote(
        std::make_shared<messages::SendSignedVote>(signed_vote));
  }
  void ProductionModuleImpl::on_slot_interval_two_started(
      std::shared_ptr<const messages::SlotIntervalTwoStarted> msg) {
    SL_INFO(logger_, "Slot interval two started on slot {}", msg->slot);
    fork_choice_store_->updateSafeTarget();
  }
  void ProductionModuleImpl::on_slot_interval_three_started(
      std::shared_ptr<const messages::SlotIntervalThreeStarted> msg) {
    SL_INFO(logger_, "Slot interval three started on slot {}", msg->slot);
    fork_choice_store_->acceptNewVotes();
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
