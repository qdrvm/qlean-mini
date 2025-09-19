/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/fork_choice.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <format>

#include "qtils/test/outcome.hpp"

using lean::Block;
using lean::Checkpoint;
using lean::ForkChoiceStore;
using lean::getForkChoiceHead;
using lean::INTERVALS_PER_SLOT;
using lean::SignedVote;

lean::BlockHash testHash(std::string_view s) {
  lean::BlockHash hash;
  EXPECT_LE(s.size(), hash.size());
  memcpy(hash.data(), s.data(), s.size());
  return hash;
}

SignedVote makeVote(const Block &source, const Block &target) {
  return SignedVote{
      .data =
          {
              .slot = target.slot,
              .head = Checkpoint::from(target),
              .target = Checkpoint::from(target),
              .source = Checkpoint::from(source),
          },
  };
}

std::optional<Checkpoint> getVote(const ForkChoiceStore::Votes &votes) {
  auto it = votes.find(0);
  if (it == votes.end()) {
    return std::nullopt;
  }
  return it->second;
}

lean::Config config{
    .num_validators = 100,
    .genesis_time = 1000,
};

auto makeBlockMap(std::vector<lean::Block> blocks) {
  ForkChoiceStore::Blocks map;
  for (auto block : blocks) {
    block.setHash();
    map.emplace(block.hash(), block);
  }
  return map;
}

std::vector<lean::Block> makeBlocks(lean::Slot count) {
  std::vector<lean::Block> blocks;
  auto parent_root = testHash("genesis-parent");
  for (lean::Slot slot = 0; slot < count; ++slot) {
    lean::Block block{
        .slot = slot,
        .parent_root = parent_root,
        .state_root = testHash(std::format("state-{}", slot)),
    };
    block.setHash();
    blocks.emplace_back(block);
    parent_root = block.hash();
  }
  return blocks;
}

// Test basic vote target selection.
TEST(TestVoteTargetCalculation, test_get_vote_target_basic) {
  auto blocks = makeBlocks(2);
  auto &genesis = blocks.at(0);
  auto &block_1 = blocks.at(1);

  // Recent finalization
  auto finalized = Checkpoint::from(genesis);

  ForkChoiceStore store{
      .time_ = 100,
      .config_ = config,
      .head_ = block_1.hash(),
      .safe_target_ = block_1.hash(),
      .latest_justified_ = finalized,
      .latest_finalized_ = finalized,
      .blocks_ = makeBlockMap(blocks),
  };

  auto target = store.getVoteTarget();

  // Should target the head block since finalization is recent
  EXPECT_EQ(target.root, block_1.hash());
  EXPECT_EQ(target.slot, 1);
}

// Test vote target selection with very old finalized checkpoint.
TEST(TestVoteTargetCalculation, test_vote_target_with_old_finalized) {
  auto blocks = makeBlocks(10);

  // Very old finalized checkpoint (slot 0)
  auto finalized = Checkpoint::from(blocks.at(0));

  // Current head is at slot 9
  auto &head = blocks.at(9);

  ForkChoiceStore store{
      .time_ = 100,
      .config_ = config,
      .head_ = head.hash(),
      .safe_target_ = head.hash(),
      .latest_justified_ = finalized,
      .latest_finalized_ = finalized,
      .blocks_ = makeBlockMap(blocks),
  };

  auto target = store.getVoteTarget();

  // Should return a valid checkpoint
  EXPECT_TRUE(store.blocks_.contains(target.root));
}

// Test that vote target walks back from head when needed.
TEST(TestVoteTargetCalculation, test_vote_target_walks_back_from_head) {
  auto blocks = makeBlocks(3);
  auto &genesis = blocks.at(0);
  auto &block_1 = blocks.at(1);
  auto &block_2 = blocks.at(2);

  // Finalized at genesis
  auto finalized = Checkpoint::from(genesis);

  ForkChoiceStore store{
      .time_ = 100,
      .config_ = config,
      .head_ = block_2.hash(),
      // Different from head
      .safe_target_ = block_1.hash(),
      .latest_justified_ = finalized,
      .latest_finalized_ = finalized,
      .blocks_ = makeBlockMap(blocks),
  };

  auto target = store.getVoteTarget();

  // Should walk back towards safe target
  EXPECT_TRUE(store.blocks_.contains(target.root));
}

// Test that vote target respects justifiable slot constraints.
TEST(TestVoteTargetCalculation, test_vote_target_justifiable_slot_constraint) {
  // Create a long chain to test slot justification
  auto blocks = makeBlocks(21);

  // Finalized very early (slot 0)
  auto finalized = Checkpoint::from(blocks.at(0));

  // Head at slot 20
  auto &head = blocks.at(20);

  ForkChoiceStore store{
      .time_ = 100,
      .config_ = config,
      .head_ = head.hash(),
      .safe_target_ = head.hash(),
      .latest_justified_ = finalized,
      .latest_finalized_ = finalized,
      .blocks_ = makeBlockMap(blocks),
  };

  auto target = store.getVoteTarget();

  // Should return a justifiable slot
  EXPECT_TRUE(store.blocks_.contains(target.root));

  // Check that the slot is justifiable after finalized slot
  EXPECT_TRUE(lean::isJustifiableSlot(finalized.slot, target.slot));
}

// Test vote target when head and safe_target are the same.
TEST(TestVoteTargetCalculation,
     test_vote_target_with_same_head_and_safe_target) {
  auto blocks = makeBlocks(2);
  auto &genesis = blocks.at(0);
  auto &head = blocks.at(1);

  auto finalized = Checkpoint::from(genesis);

  ForkChoiceStore store{
      .time_ = 500,
      .config_ = config,
      .head_ = head.hash(),
      // Same as head
      .safe_target_ = head.hash(),
      .latest_justified_ = finalized,
      .latest_finalized_ = finalized,
      .blocks_ = makeBlockMap(blocks),
  };

  auto target = store.getVoteTarget();

  // Should target the head (which is also safe_target)
  EXPECT_EQ(target.root, head.hash());
  EXPECT_EQ(target.slot, head.slot);
}

// Test get_fork_choice_head with validator votes.
TEST(TestForkChoiceHeadFunction, test_get_fork_choice_head_with_votes) {
  auto blocks = makeBlocks(3);
  auto &root = blocks.at(0);
  auto &target = blocks.at(2);

  auto head = getForkChoiceHead(makeBlockMap(blocks),
                                Checkpoint::from(root),
                                {{0, Checkpoint::from(target)}},
                                0);

  EXPECT_EQ(head, target.hash());
}

// Test get_fork_choice_head with no votes returns the root.
TEST(TestForkChoiceHeadFunction, test_get_fork_choice_head_no_votes) {
  auto blocks = makeBlocks(3);
  auto &root = blocks.at(0);

  auto head =
      getForkChoiceHead(makeBlockMap(blocks), Checkpoint::from(root), {}, 0);

  EXPECT_EQ(head, root.hash());
}

// Test get_fork_choice_head respects minimum score.
TEST(TestForkChoiceHeadFunction, test_get_fork_choice_head_with_min_score) {
  auto blocks = makeBlocks(3);
  auto &root = blocks.at(0);
  auto &target = blocks.at(2);

  auto head = getForkChoiceHead(makeBlockMap(blocks),
                                Checkpoint::from(root),
                                {{0, Checkpoint::from(target)}},
                                2);

  EXPECT_EQ(head, root.hash());
}

// Test get_fork_choice_head with multiple votes.
TEST(TestForkChoiceHeadFunction, test_get_fork_choice_head_multiple_votes) {
  auto blocks = makeBlocks(3);
  auto &root = blocks.at(0);
  auto &target = blocks.at(2);

  auto head = getForkChoiceHead(makeBlockMap(blocks),
                                Checkpoint::from(root),
                                {
                                    {0, Checkpoint::from(target)},
                                    {1, Checkpoint::from(target)},
                                    {2, Checkpoint::from(target)},
                                },
                                0);

  EXPECT_EQ(head, target.hash());
}

// Test basic safe target update.
TEST(TestSafeTargetComputation, test_update_safe_target_basic) {
  auto blocks = makeBlocks(1);
  auto &genesis = blocks.at(0);

  auto finalized = Checkpoint::from(genesis);

  ForkChoiceStore store{
      .time_ = 100,
      .config_ = config,
      .head_ = genesis.hash(),
      .safe_target_ = genesis.hash(),
      .latest_justified_ = finalized,
      .latest_finalized_ = finalized,
      .blocks_ = makeBlockMap(blocks),
  };

  // Update safe target (this tests the method exists and runs)
  store.updateSafeTarget();

  // Safe target should be set
  EXPECT_EQ(store.safe_target_, genesis.hash());
}

// Test safe target computation with votes.
TEST(TestSafeTargetComputation, test_safe_target_with_votes) {
  auto blocks = makeBlocks(2);
  auto &genesis = blocks.at(0);
  auto &block_1 = blocks.at(1);

  auto finalized = Checkpoint::from(genesis);

  ForkChoiceStore store{
      .time_ = 100,
      .config_ = config,
      .head_ = block_1.hash(),
      .safe_target_ = genesis.hash(),
      .latest_justified_ = finalized,
      .latest_finalized_ = finalized,
      .blocks_ = makeBlockMap(blocks),
      // Add some new votes
      .latest_new_votes_ =
          {
              {0, Checkpoint::from(block_1)},
              {1, Checkpoint::from(block_1)},
          },
  };

  // Update safe target with votes
  store.updateSafeTarget();

  // Should have computed a safe target
  EXPECT_TRUE(store.blocks_.contains(store.safe_target_));
}

// Test vote target with only one block.
TEST(TestEdgeCases, test_vote_target_single_block) {
  auto blocks = makeBlocks(1);
  auto &genesis = blocks.at(0);

  auto finalized = Checkpoint::from(genesis);

  ForkChoiceStore store{
      .time_ = 100,
      .config_ = config,
      .head_ = genesis.hash(),
      .safe_target_ = genesis.hash(),
      .latest_justified_ = finalized,
      .latest_finalized_ = finalized,
      .blocks_ = makeBlockMap(blocks),
  };

  auto target = store.getVoteTarget();

  EXPECT_EQ(target.root, genesis.hash());
  EXPECT_EQ(target.slot, genesis.slot);
}

// Test validation of a valid attestation.
TEST(TestAttestationValidation, test_validate_attestation_valid) {
  auto blocks = makeBlocks(3);
  auto &source = blocks.at(1);
  auto &target = blocks.at(2);

  ForkChoiceStore sample_store{
      .time_ = 100,
      .blocks_ = makeBlockMap(blocks),
  };

  // Create valid signed vote
  // Should validate without error
  EXPECT_OUTCOME_SUCCESS(
      sample_store.validateAttestation(makeVote(source, target)));
}

// Test validation fails when source slot > target slot.
TEST(TestAttestationValidation, test_validate_attestation_slot_order_invalid) {
  auto blocks = makeBlocks(3);
  // Later than target
  auto &source = blocks.at(2);
  // Earlier than source
  auto &target = blocks.at(1);

  ForkChoiceStore sample_store{
      .time_ = 100,
      .blocks_ = makeBlockMap(blocks),
  };

  // Create invalid signed vote (source > target slot)
  EXPECT_OUTCOME_ERROR(
      sample_store.validateAttestation(makeVote(source, target)));
}

// Test validation fails when referenced blocks are missing.
TEST(TestAttestationValidation, test_validate_attestation_missing_blocks) {
  ForkChoiceStore sample_store;

  // Create signed vote referencing missing blocks
  EXPECT_OUTCOME_ERROR(sample_store.validateAttestation({}));
}

// Test validation fails when checkpoint slots don't match block slots.
TEST(TestAttestationValidation,
     test_validate_attestation_checkpoint_slot_mismatch) {
  auto blocks = makeBlocks(3);
  auto &source = blocks.at(1);
  auto &target = blocks.at(2);

  ForkChoiceStore sample_store{
      .time_ = 100,
      .blocks_ = makeBlockMap(blocks),
  };

  // Create signed vote with mismatched checkpoint slot
  auto vote = makeVote(source, target);
  ++vote.data.source.slot;
  EXPECT_OUTCOME_ERROR(sample_store.validateAttestation(vote));
}

// Test validation fails for attestations too far in the future.
TEST(TestAttestationValidation, test_validate_attestation_too_far_future) {
  auto blocks = makeBlocks(10);
  auto &source = blocks.at(1);
  auto &target = blocks.at(9);

  ForkChoiceStore sample_store{
      .blocks_ = makeBlockMap(blocks),
  };

  // Create signed vote for future slot
  EXPECT_OUTCOME_ERROR(
      sample_store.validateAttestation(makeVote(source, target)));
}

// Test processing attestation from network gossip.
TEST(TestAttestationProcessing, test_process_network_attestation) {
  auto blocks = makeBlocks(3);
  auto &source = blocks.at(1);
  auto &target = blocks.at(2);

  ForkChoiceStore sample_store{
      .time_ = 100,
      .blocks_ = makeBlockMap(blocks),
  };

  // Create valid signed vote
  // Process as network attestation
  EXPECT_OUTCOME_SUCCESS(
      sample_store.processAttestation(makeVote(source, target), false));

  // Vote should be added to new votes
  EXPECT_EQ(getVote(sample_store.latest_new_votes_), Checkpoint::from(target));
}

// Test processing attestation from a block.
TEST(TestAttestationProcessing, test_process_block_attestation) {
  auto blocks = makeBlocks(3);
  auto &source = blocks.at(1);
  auto &target = blocks.at(2);

  ForkChoiceStore sample_store{
      .time_ = 100,
      .blocks_ = makeBlockMap(blocks),
  };

  // Create valid signed vote
  // Process as block attestation
  EXPECT_OUTCOME_SUCCESS(
      sample_store.processAttestation(makeVote(source, target), true));

  // Vote should be added to known votes
  EXPECT_EQ(getVote(sample_store.latest_known_votes_),
            Checkpoint::from(target));
}

// Test that newer attestations supersede older ones.
TEST(TestAttestationProcessing, test_process_attestation_superseding) {
  auto blocks = makeBlocks(3);
  auto &target_1 = blocks.at(1);
  auto &target_2 = blocks.at(2);

  ForkChoiceStore sample_store{
      .time_ = 100,
      .blocks_ = makeBlockMap(blocks),
  };

  // Process first (older) attestation
  EXPECT_OUTCOME_SUCCESS(
      sample_store.processAttestation(makeVote(target_1, target_1), false));

  // Process second (newer) attestation
  EXPECT_OUTCOME_SUCCESS(
      sample_store.processAttestation(makeVote(target_1, target_2), false));

  // Should have the newer vote
  EXPECT_EQ(getVote(sample_store.latest_new_votes_),
            Checkpoint::from(target_2));
}

// Test that block attestations remove corresponding new votes.
TEST(TestAttestationProcessing,
     test_process_attestation_from_block_supersedes_new) {
  auto blocks = makeBlocks(3);
  auto &source = blocks.at(1);
  auto &target = blocks.at(2);

  ForkChoiceStore sample_store{
      .time_ = 100,
      .blocks_ = makeBlockMap(blocks),
  };

  // First process as network vote
  auto signed_vote = makeVote(source, target);
  EXPECT_OUTCOME_SUCCESS(sample_store.processAttestation(signed_vote, false));

  // Should be in new votes
  ASSERT_TRUE(getVote(sample_store.latest_new_votes_));

  // Process same vote as block attestation
  EXPECT_OUTCOME_SUCCESS(sample_store.processAttestation(signed_vote, true));

  // Vote should move to known votes and be removed from new votes
  ASSERT_FALSE(getVote(sample_store.latest_new_votes_));
  EXPECT_EQ(getVote(sample_store.latest_known_votes_),
            Checkpoint::from(target));
}

// Test basic time advancement.
TEST(TestTimeAdvancement, test_advance_time_basic) {
  ForkChoiceStore sample_store{
      .time_ = 100,
  };

  auto initial_time = sample_store.time_;
  // Much later time
  auto target_time = sample_store.config_.genesis_time + 200;

  sample_store.advanceTime(target_time, true);

  // Time should advance
  EXPECT_GT(sample_store.time_, initial_time);
}

// Test time advancement without proposal.
TEST(TestTimeAdvancement, test_advance_time_no_proposal) {
  ForkChoiceStore sample_store{
      .time_ = 100,
  };

  auto initial_time = sample_store.time_;
  auto target_time = sample_store.config_.genesis_time + 100;

  sample_store.advanceTime(target_time, false);

  // Time should still advance
  EXPECT_GE(sample_store.time_, initial_time);
}

// Test advance_time when already at target time.
TEST(TestTimeAdvancement, test_advance_time_already_current) {
  ForkChoiceStore sample_store{
      .time_ = 100,
  };

  auto initial_time = sample_store.time_;
  auto current_target = sample_store.config_.genesis_time + initial_time;

  // Try to advance to current time (should be no-op)
  sample_store.advanceTime(current_target, false);

  // Should not change significantly
  EXPECT_LE(std::abs<int>(sample_store.time_ - initial_time), 10);
}

// Test advance_time with small time increment.
TEST(TestTimeAdvancement, test_advance_time_small_increment) {
  ForkChoiceStore sample_store{
      .time_ = 100,
  };

  auto initial_time = sample_store.time_;
  auto target_time = sample_store.config_.genesis_time + initial_time + 1;

  sample_store.advanceTime(target_time, false);

  // Should advance by small amount
  EXPECT_GE(sample_store.time_, initial_time);
}

// Test basic interval ticking.
TEST(TestIntervalTicking, test_tick_interval_basic) {
  ForkChoiceStore sample_store{
      .time_ = 100,
  };
  auto initial_time = sample_store.time_;

  // Tick one interval forward
  sample_store.tickInterval(false);

  // Time should advance by one interval
  EXPECT_EQ(sample_store.time_, initial_time + 1);
}

// Test interval ticking with proposal.
TEST(TestIntervalTicking, test_tick_interval_with_proposal) {
  ForkChoiceStore sample_store{
      .time_ = 100,
  };
  auto initial_time = sample_store.time_;

  sample_store.tickInterval(false);

  // Time should advance
  EXPECT_EQ(sample_store.time_, initial_time + 1);
}

// Test sequence of interval ticks.
TEST(TestIntervalTicking, test_tick_interval_sequence) {
  ForkChoiceStore sample_store{
      .time_ = 100,
  };
  auto initial_time = sample_store.time_;

  // Tick multiple intervals
  for (auto i = 0; i < 5; ++i) {
    sample_store.tickInterval(i % 2 == 0);
  }

  // Should have advanced by 5 intervals
  EXPECT_EQ(sample_store.time_, initial_time + 5);
}

// Test different actions performed based on interval phase.
TEST(TestIntervalTicking, test_tick_interval_actions_by_phase) {
  // Reset store to known state
  ForkChoiceStore sample_store;

  // Add some test votes for processing
  sample_store.latest_new_votes_.emplace(0, Checkpoint{.slot = 1});

  // Tick through a complete slot cycle
  for (lean::Interval interval = 0; interval < INTERVALS_PER_SLOT; ++interval) {
    // Proposal only in first interval
    auto has_proposal = interval == 0;
    sample_store.tickInterval(has_proposal);

    auto current_interval = sample_store.time_ % INTERVALS_PER_SLOT;
    auto expected_interval = (interval + 1) % INTERVALS_PER_SLOT;
    EXPECT_EQ(current_interval, expected_interval);
  }
}

// Test getting proposal head for a slot.
TEST(TestProposalHeadTiming, test_get_proposal_head_basic) {
  auto blocks = makeBlocks(1);
  auto &genesis = blocks.at(0);
  ForkChoiceStore sample_store{
      .time_ = 100,
      .config_ = config,
      .head_ = genesis.hash(),
      .blocks_ = makeBlockMap(blocks),
  };

  // Get proposal head for slot 0
  auto head = sample_store.getProposalHead(genesis.slot);

  // Should return current head
  EXPECT_EQ(head, sample_store.head_);
}

// Test that get_proposal_head advances store time appropriately.
TEST(TestProposalHeadTiming, test_get_proposal_head_advances_time) {
  ForkChoiceStore sample_store{
      .time_ = 100,
  };
  auto initial_time = sample_store.time_;

  // Get proposal head for a future slot
  sample_store.getProposalHead(5);

  // Time may have advanced (depending on slot timing)
  // This is mainly testing that the call doesn't fail
  EXPECT_GE(sample_store.time_, initial_time);
}

// Test that get_proposal_head processes pending votes.
TEST(TestProposalHeadTiming, test_get_proposal_head_processes_votes) {
  ForkChoiceStore sample_store{
      .time_ = 100,
  };

  // Add some new votes
  sample_store.latest_new_votes_.emplace(0, Checkpoint{.slot = 1});

  // Get proposal head should process votes
  sample_store.getProposalHead(1);

  // Votes should have been processed (moved to known votes)
  ASSERT_FALSE(getVote(sample_store.latest_new_votes_));
  ASSERT_TRUE(getVote(sample_store.latest_known_votes_));
}
