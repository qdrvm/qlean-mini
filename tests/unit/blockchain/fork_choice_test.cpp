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
#include <memory>

#include "mock/clock/manual_clock.hpp"
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

std::shared_ptr<lean::clock::ManualClock> createManualClock(
    uint64_t time_msec = 1000) {
  return std::make_shared<lean::clock::ManualClock>(time_msec);
}

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
  auto mock_clock = createManualClock(100);

  ForkChoiceStore store(mock_clock,           // clock
                        config,               // config
                        block_1.hash(),       // head
                        block_1.hash(),       // safe_target
                        finalized,            // latest_justified
                        finalized,            // latest_finalized
                        makeBlockMap(blocks)  // blocks
  );

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
  auto mock_clock = createManualClock(100);

  ForkChoiceStore store(mock_clock,           // clock
                        config,               // config
                        head.hash(),          // head
                        head.hash(),          // safe_target
                        finalized,            // latest_justified
                        finalized,            // latest_finalized
                        makeBlockMap(blocks)  // blocks
  );

  auto target = store.getVoteTarget();

  // Should return a valid checkpoint
  EXPECT_TRUE(store.getBlocks().contains(target.root));
}

// Test that vote target walks back from head when needed.
TEST(TestVoteTargetCalculation, test_vote_target_walks_back_from_head) {
  auto blocks = makeBlocks(3);
  auto &genesis = blocks.at(0);
  auto &block_1 = blocks.at(1);
  auto &block_2 = blocks.at(2);

  // Finalized at genesis
  auto finalized = Checkpoint::from(genesis);
  auto mock_clock = createManualClock(100);

  ForkChoiceStore store(mock_clock,      // clock
                        config,          // config
                        block_2.hash(),  // head
                        block_1.hash(),  // safe_target (different from head)
                        finalized,       // latest_justified
                        finalized,       // latest_finalized
                        makeBlockMap(blocks)  // blocks
  );

  auto target = store.getVoteTarget();

  // Should walk back towards safe target
  EXPECT_TRUE(store.getBlocks().contains(target.root));
}

// Test that vote target respects justifiable slot constraints.
TEST(TestVoteTargetCalculation, test_vote_target_justifiable_slot_constraint) {
  // Create a long chain to test slot justification
  auto blocks = makeBlocks(21);

  // Finalized very early (slot 0)
  auto finalized = Checkpoint::from(blocks.at(0));

  // Head at slot 20
  auto &head = blocks.at(20);

  auto mock_clock = createManualClock(100);
  ForkChoiceStore store(mock_clock,           // clock
                        config,               // config
                        head.hash(),          // head
                        head.hash(),          // safe_target
                        finalized,            // latest_justified
                        finalized,            // latest_finalized
                        makeBlockMap(blocks)  // blocks
  );

  auto target = store.getVoteTarget();

  // Should return a justifiable slot
  EXPECT_TRUE(store.getBlocks().contains(target.root));

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
  auto mock_clock = createManualClock(500);

  ForkChoiceStore store(mock_clock,           // clock
                        config,               // config
                        head.hash(),          // head
                        head.hash(),          // safe_target (same as head)
                        finalized,            // latest_justified
                        finalized,            // latest_finalized
                        makeBlockMap(blocks)  // blocks
  );

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
  auto mock_clock = createManualClock(100);

  ForkChoiceStore store(mock_clock,           // clock
                        config,               // config
                        genesis.hash(),       // head
                        genesis.hash(),       // safe_target
                        finalized,            // latest_justified
                        finalized,            // latest_finalized
                        makeBlockMap(blocks)  // blocks
  );

  // Update safe target (this tests the method exists and runs)
  store.updateSafeTarget();

  // Safe target should be set
  EXPECT_EQ(store.getSafeTarget(), genesis.hash());
}

// Test safe target computation with votes.
TEST(TestSafeTargetComputation, test_safe_target_with_votes) {
  auto blocks = makeBlocks(2);
  auto &genesis = blocks.at(0);
  auto &block_1 = blocks.at(1);

  auto finalized = Checkpoint::from(genesis);
  auto mock_clock = createManualClock(100);

  ForkChoiceStore store(mock_clock,            // clock
                        config,                // config
                        block_1.hash(),        // head
                        genesis.hash(),        // safe_target
                        finalized,             // latest_justified
                        finalized,             // latest_finalized
                        makeBlockMap(blocks),  // blocks
                        {},                    // states
                        {},                    // latest_known_votes
                        {},                    // signed_votes
                        {
                            // latest_new_votes
                            {0, Checkpoint::from(block_1)},
                            {1, Checkpoint::from(block_1)},
                        });

  // Update safe target with votes
  store.updateSafeTarget();

  // Should have computed a safe target
  EXPECT_TRUE(store.getBlocks().contains(store.getSafeTarget()));
}

// Test vote target with only one block.
TEST(TestEdgeCases, test_vote_target_single_block) {
  auto blocks = makeBlocks(1);
  auto &genesis = blocks.at(0);

  auto finalized = Checkpoint::from(genesis);
  auto mock_clock = createManualClock(100);

  ForkChoiceStore store(mock_clock,           // clock
                        config,               // config
                        genesis.hash(),       // head
                        genesis.hash(),       // safe_target
                        finalized,            // latest_justified
                        finalized,            // latest_finalized
                        makeBlockMap(blocks)  // blocks
  );

  auto target = store.getVoteTarget();

  EXPECT_EQ(target.root, genesis.hash());
  EXPECT_EQ(target.slot, genesis.slot);
}

// Test validation of a valid attestation.
TEST(TestAttestationValidation, test_validate_attestation_valid) {
  auto blocks = makeBlocks(3);
  auto &source = blocks.at(1);
  auto &target = blocks.at(2);

  auto mock_clock = createManualClock(100);
  ForkChoiceStore sample_store(mock_clock,           // clock
                               config,               // config
                               {},                   // head
                               {},                   // safe_target
                               {},                   // latest_justified
                               {},                   // latest_finalized
                               makeBlockMap(blocks)  // blocks
  );

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

  auto mock_clock = createManualClock(100);
  ForkChoiceStore sample_store(mock_clock,           // clock
                               {},                   // config
                               {},                   // head
                               {},                   // safe_target
                               {},                   // latest_justified
                               {},                   // latest_finalized
                               makeBlockMap(blocks)  // blocks
  );

  // Create invalid signed vote (source > target slot)
  EXPECT_OUTCOME_ERROR(
      sample_store.validateAttestation(makeVote(source, target)));
}

// Test validation fails when referenced blocks are missing.
TEST(TestAttestationValidation, test_validate_attestation_missing_blocks) {
  auto mock_clock = createManualClock(0);
  ForkChoiceStore sample_store(mock_clock);

  // Create signed vote referencing missing blocks
  EXPECT_OUTCOME_ERROR(sample_store.validateAttestation({}));
}

// Test validation fails when checkpoint slots don't match block slots.
TEST(TestAttestationValidation,
     test_validate_attestation_checkpoint_slot_mismatch) {
  auto blocks = makeBlocks(3);
  auto &source = blocks.at(1);
  auto &target = blocks.at(2);

  auto mock_clock = createManualClock(100);
  ForkChoiceStore sample_store(mock_clock,           // clock
                               {},                   // config
                               {},                   // head
                               {},                   // safe_target
                               {},                   // latest_justified
                               {},                   // latest_finalized
                               makeBlockMap(blocks)  // blocks
  );

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

  auto mock_clock = createManualClock(0);
  ForkChoiceStore sample_store(mock_clock,           // clock
                               {},                   // config
                               {},                   // head
                               {},                   // safe_target
                               {},                   // latest_justified
                               {},                   // latest_finalized
                               makeBlockMap(blocks)  // blocks
  );

  // Create signed vote for future slot
  EXPECT_OUTCOME_ERROR(
      sample_store.validateAttestation(makeVote(source, target)));
}

// Test processing attestation from network gossip.
TEST(TestAttestationProcessing, test_process_network_attestation) {
  auto blocks = makeBlocks(3);
  auto &source = blocks.at(1);
  auto &target = blocks.at(2);

  auto mock_clock = createManualClock(100);
  ForkChoiceStore sample_store(mock_clock,           // clock
                               config,               // config
                               {},                   // head
                               {},                   // safe_target
                               {},                   // latest_justified
                               {},                   // latest_finalized
                               makeBlockMap(blocks)  // blocks
  );

  // Create valid signed vote
  // Process as network attestation
  EXPECT_OUTCOME_SUCCESS(
      sample_store.processAttestation(makeVote(source, target), false));

  // Vote should be added to new votes
  EXPECT_EQ(getVote(sample_store.getLatestNewVotes()),
            Checkpoint::from(target));
}

// Test processing attestation from a block.
TEST(TestAttestationProcessing, test_process_block_attestation) {
  auto blocks = makeBlocks(3);
  auto &source = blocks.at(1);
  auto &target = blocks.at(2);

  auto mock_clock = createManualClock(100);
  ForkChoiceStore sample_store(mock_clock,           // clock
                               config,               // config
                               {},                   // head
                               {},                   // safe_target
                               {},                   // latest_justified
                               {},                   // latest_finalized
                               makeBlockMap(blocks)  // blocks
  );

  // Create valid signed vote
  // Process as block attestation
  EXPECT_OUTCOME_SUCCESS(
      sample_store.processAttestation(makeVote(source, target), true));

  // Vote should be added to known votes
  EXPECT_EQ(getVote(sample_store.getLatestKnownVotes()),
            Checkpoint::from(target));
}

// Test that newer attestations supersede older ones.
TEST(TestAttestationProcessing, test_process_attestation_superseding) {
  auto blocks = makeBlocks(3);
  auto &target_1 = blocks.at(1);
  auto &target_2 = blocks.at(2);

  auto mock_clock = createManualClock(100);
  ForkChoiceStore sample_store(mock_clock,           // clock
                               config,               // config
                               {},                   // head
                               {},                   // safe_target
                               {},                   // latest_justified
                               {},                   // latest_finalized
                               makeBlockMap(blocks)  // blocks
  );

  // Process first (older) attestation
  EXPECT_OUTCOME_SUCCESS(
      sample_store.processAttestation(makeVote(target_1, target_1), false));

  // Process second (newer) attestation
  EXPECT_OUTCOME_SUCCESS(
      sample_store.processAttestation(makeVote(target_1, target_2), false));

  // Should have the newer vote
  EXPECT_EQ(getVote(sample_store.getLatestNewVotes()),
            Checkpoint::from(target_2));
}

// Test that block attestations remove corresponding new votes.
TEST(TestAttestationProcessing,
     test_process_attestation_from_block_supersedes_new) {
  auto blocks = makeBlocks(3);
  auto &source = blocks.at(1);
  auto &target = blocks.at(2);

  auto mock_clock = createManualClock(100);
  ForkChoiceStore sample_store(mock_clock,           // clock
                               config,               // config
                               {},                   // head
                               {},                   // safe_target
                               {},                   // latest_justified
                               {},                   // latest_finalized
                               makeBlockMap(blocks)  // blocks
  );

  // First process as network vote
  auto signed_vote = makeVote(source, target);
  EXPECT_OUTCOME_SUCCESS(sample_store.processAttestation(signed_vote, false));

  // Should be in new votes
  ASSERT_TRUE(getVote(sample_store.getLatestNewVotes()));

  // Process same vote as block attestation
  EXPECT_OUTCOME_SUCCESS(sample_store.processAttestation(signed_vote, true));

  // Vote should move to known votes and be removed from new votes
  ASSERT_FALSE(getVote(sample_store.getLatestNewVotes()));
  EXPECT_EQ(getVote(sample_store.getLatestKnownVotes()),
            Checkpoint::from(target));
}

// Test basic time advancement.
TEST(TestTimeAdvancement, test_advance_time_basic) {
  auto mock_clock =
      createManualClock(config.genesis_time + 4000);  // 4 seconds after genesis
  ForkChoiceStore sample_store(mock_clock, config);

  auto current_slot = sample_store.getCurrentSlot();
  // Should be slot 1 (4000ms after genesis_time = slot 1)
  EXPECT_EQ(current_slot, 1);

  // Advance clock and check slot advancement
  mock_clock->advance(4000);  // Another slot
  auto new_slot = sample_store.getCurrentSlot();
  EXPECT_EQ(new_slot, 2);
}

// Test time advancement without proposal.
TEST(TestTimeAdvancement, test_advance_time_no_proposal) {
  auto mock_clock = createManualClock(config.genesis_time);
  ForkChoiceStore sample_store(mock_clock, config);

  auto initial_slot = sample_store.getCurrentSlot();
  EXPECT_EQ(initial_slot, 0);

  mock_clock->setTime(config.genesis_time
                      + 20000);  // 20 seconds later (5 slots)
  auto new_slot = sample_store.getCurrentSlot();
  EXPECT_EQ(new_slot, 5);
}

// Test clock mock precision.
TEST(TestTimeAdvancement, test_advance_time_small_increment) {
  auto mock_clock = createManualClock(config.genesis_time + 2000);  // 2 seconds
  ForkChoiceStore sample_store(mock_clock, config);

  auto slot = sample_store.getCurrentSlot();
  EXPECT_EQ(slot, 0);  // Should still be slot 0 (2000ms < 4000ms)

  // Advance by 2000ms more to reach 1 full slot
  mock_clock->advance(2000);
  slot = sample_store.getCurrentSlot();
  EXPECT_EQ(slot, 1);  // Now should be slot 1
}

// Test clock mock incremental advancement.
TEST(TestTimeAdvancement, test_advance_time_already_current) {
  auto mock_clock = createManualClock(config.genesis_time);
  ForkChoiceStore sample_store(mock_clock, config);

  // Test small incremental advances
  for (int i = 0; i < 40; ++i) {
    mock_clock->advance(100);  // 0.1 second increments
    auto expected_slot =
        (i + 1) / 40;  // Should change slot every 40 increments (4000ms)
    EXPECT_EQ(sample_store.getCurrentSlot(), expected_slot);
  }
}

// Test basic interval ticking.
TEST(TestIntervalTicking, test_tick_interval_basic) {
  auto mock_clock = createManualClock(config.genesis_time);
  ForkChoiceStore sample_store(mock_clock, config);

  auto initial_slot = sample_store.getCurrentSlot();
  EXPECT_EQ(initial_slot, 0);

  // Advance clock by one slot duration
  mock_clock->advance(4000);  // 4 seconds = 1 slot
  auto new_slot = sample_store.getCurrentSlot();
  EXPECT_EQ(new_slot, 1);
}

// Test interval ticking with proposal.
TEST(TestIntervalTicking, test_tick_interval_with_proposal) {
  auto mock_clock =
      createManualClock(config.genesis_time + 8000);  // 8 seconds after genesis
  ForkChoiceStore sample_store(mock_clock, config);

  auto slot = sample_store.getCurrentSlot();
  EXPECT_EQ(slot, 2);  // Should be in slot 2 (8000/4000 = 2)

  mock_clock->setTime(config.genesis_time + 40000);  // 40 seconds
  slot = sample_store.getCurrentSlot();
  EXPECT_EQ(slot, 10);  // Should be in slot 10 (40000/4000 = 10)
}

// Test sequence of interval ticks.
TEST(TestIntervalTicking, test_tick_interval_sequence) {
  auto mock_clock = createManualClock(config.genesis_time);
  ForkChoiceStore sample_store(mock_clock, config);

  // Test multiple sequential advances
  for (auto i = 0; i < 5; ++i) {
    mock_clock->advance(4000);  // 4 seconds each = 1 slot
    auto expected_slot = i + 1;
    EXPECT_EQ(sample_store.getCurrentSlot(), expected_slot);
  }
}

// Test different actions performed based on interval phase.
TEST(TestIntervalTicking, test_tick_interval_actions_by_phase) {
  auto mock_clock = createManualClock(config.genesis_time);
  ForkChoiceStore sample_store(mock_clock, config);

  // Add some test votes for processing
  sample_store.getLatestNewVotesRef().emplace(0, Checkpoint{.slot = 1});

  // Test that clock state is preserved across operations
  auto initial_slot = sample_store.getCurrentSlot();
  EXPECT_EQ(initial_slot, 0);

  // Advance time and verify it's persistent
  mock_clock->advance(12000);  // 12 seconds = 3 slots
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(sample_store.getCurrentSlot(),
              3);  // Should consistently return slot 3
  }
}

// Test getting proposal head for a slot.
TEST(TestProposalHeadTiming, test_get_proposal_head_basic) {
  auto blocks = makeBlocks(1);
  auto &genesis = blocks.at(0);
  auto mock_clock = createManualClock(100);
  ForkChoiceStore sample_store(mock_clock,           // clock
                               config,               // config
                               genesis.hash(),       // head
                               {},                   // safe_target
                               {},                   // latest_justified
                               {},                   // latest_finalized
                               makeBlockMap(blocks)  // blocks
  );

  // Get proposal head
  auto head = sample_store.getHead();

  // Should return current head
  EXPECT_EQ(head, genesis.hash());
}

// Test that getHead works with clock.
TEST(TestProposalHeadTiming, test_get_head_with_clock) {
  auto mock_clock = createManualClock(100);
  ForkChoiceStore sample_store(mock_clock);

  // Get proposal head
  auto head = sample_store.getHead();

  // Since we have an empty store, head should be empty hash
  EXPECT_EQ(head, lean::BlockHash{});

  // Test that clock time is accessible through getCurrentSlot
  auto slot = sample_store.getCurrentSlot();
  EXPECT_EQ(slot, 0);  // At genesis time + 100ms = still slot 0
}

// Test that head calculation works with votes.
TEST(TestProposalHeadTiming, test_head_calculation_with_votes) {
  auto mock_clock =
      createManualClock(config.genesis_time + 4000);  // 1 slot after genesis
  ForkChoiceStore sample_store(mock_clock, config);

  // Add some new votes
  sample_store.getLatestNewVotesRef().emplace(0, Checkpoint{.slot = 1});

  // Get proposal head
  auto head = sample_store.getHead();

  // Test that votes are still accessible (they won't be processed
  // automatically)
  ASSERT_TRUE(getVote(sample_store.getLatestNewVotes()));

  // Test clock functionality
  EXPECT_EQ(sample_store.getCurrentSlot(), 1);  // Should be slot 1 initially
}
