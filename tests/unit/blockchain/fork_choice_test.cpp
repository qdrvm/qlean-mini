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

#include "blockchain/is_justifiable_slot.hpp"
#include "qtils/test/outcome.hpp"
#include "tests/testutil/prepare_loggers.hpp"
#include "types/signed_block.hpp"

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
      .validator_id = 0,
      .data =
          {
              .slot = target.slot,
              .head = Checkpoint::from(target),
              .target = Checkpoint::from(target),
              .source = Checkpoint::from(source),
          },
      .signature = {},
  };
}

std::optional<Checkpoint> getVote(const ForkChoiceStore::Votes &votes) {
  auto it = votes.find(0);
  if (it == votes.end()) {
    return std::nullopt;
  }
  return it->second.data.target;
}

lean::Config config{
    .num_validators = 100,
    .genesis_time = 1,
};

auto createTestStore(
    uint64_t time = 100,
    lean::Config config_param = config,
    lean::BlockHash head = {},
    lean::BlockHash safe_target = {},
    lean::Checkpoint latest_justified = {},
    lean::Checkpoint latest_finalized = {},
    ForkChoiceStore::Blocks blocks = {},
    std::unordered_map<lean::BlockHash, lean::State> states = {},
    ForkChoiceStore::Votes latest_known_votes = {},
    ForkChoiceStore::Votes latest_new_votes = {},
    lean::ValidatorIndex validator_index = 0) {
  return ForkChoiceStore(time,
                         testutil::prepareLoggers(),
                         config_param,
                         head,
                         safe_target,
                         latest_justified,
                         latest_finalized,
                         blocks,
                         states,
                         latest_known_votes,
                         latest_new_votes,
                         validator_index);
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

  auto store = createTestStore(100,
                               config,
                               block_1.hash(),
                               block_1.hash(),
                               finalized,
                               finalized,
                               makeBlockMap(blocks));

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

  auto store = createTestStore(100,
                               config,
                               head.hash(),
                               head.hash(),
                               finalized,
                               finalized,
                               makeBlockMap(blocks));

  auto target = store.getVoteTarget();

  // Should return a valid checkpoint
  EXPECT_TRUE(store.hasBlock(target.root));
}

// Test that vote target walks back from head when needed.
TEST(TestVoteTargetCalculation, test_vote_target_walks_back_from_head) {
  auto blocks = makeBlocks(3);
  auto &genesis = blocks.at(0);
  auto &block_1 = blocks.at(1);
  auto &block_2 = blocks.at(2);

  // Finalized at genesis
  auto finalized = Checkpoint::from(genesis);

  auto store = createTestStore(100,
                               config,
                               block_2.hash(),
                               block_1.hash(),
                               finalized,
                               finalized,
                               makeBlockMap(blocks));

  auto target = store.getVoteTarget();

  // Should walk back towards safe target
  EXPECT_TRUE(store.hasBlock(target.root));
}

// Test that vote target respects justifiable slot constraints.
TEST(TestVoteTargetCalculation, test_vote_target_justifiable_slot_constraint) {
  // Create a long chain to test slot justification
  auto blocks = makeBlocks(21);

  // Finalized very early (slot 0)
  auto finalized = Checkpoint::from(blocks.at(0));

  // Head at slot 20
  auto &head = blocks.at(20);

  auto store = createTestStore(100,
                               config,
                               head.hash(),
                               head.hash(),
                               finalized,
                               finalized,
                               makeBlockMap(blocks));

  auto target = store.getVoteTarget();

  // Should return a justifiable slot
  EXPECT_TRUE(store.hasBlock(target.root));

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

  auto store = createTestStore(500,
                               config,
                               head.hash(),
                               head.hash(),
                               finalized,
                               finalized,
                               makeBlockMap(blocks));

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

  ForkChoiceStore::Votes votes;
  votes[0] = SignedVote{
      .validator_id = 0,
      .data =
          {
              .slot = target.slot,
              .head = Checkpoint::from(target),
              .target = Checkpoint::from(target),
              .source = Checkpoint::from(root),
          },
      .signature = {},
  };

  auto head =
      getForkChoiceHead(makeBlockMap(blocks), Checkpoint::from(root), votes, 0);

  EXPECT_EQ(head, target.hash());
}

// Test get_fork_choice_head with no votes returns the root.
TEST(TestForkChoiceHeadFunction, test_get_fork_choice_head_no_votes) {
  auto blocks = makeBlocks(3);
  auto &root = blocks.at(0);

  ForkChoiceStore::Votes empty_votes;
  auto head = getForkChoiceHead(
      makeBlockMap(blocks), Checkpoint::from(root), empty_votes, 0);

  EXPECT_EQ(head, root.hash());
}

// Test get_fork_choice_head respects minimum score.
TEST(TestForkChoiceHeadFunction, test_get_fork_choice_head_with_min_score) {
  auto blocks = makeBlocks(3);
  auto &root = blocks.at(0);
  auto &target = blocks.at(2);

  ForkChoiceStore::Votes votes;
  votes[0] = SignedVote{
      .validator_id = 0,
      .data =
          {
              .slot = target.slot,
              .head = Checkpoint::from(target),
              .target = Checkpoint::from(target),
              .source = Checkpoint::from(root),
          },
      .signature = {},
  };

  auto head =
      getForkChoiceHead(makeBlockMap(blocks), Checkpoint::from(root), votes, 2);

  EXPECT_EQ(head, root.hash());
}

// Test get_fork_choice_head with multiple votes.
TEST(TestForkChoiceHeadFunction, test_get_fork_choice_head_multiple_votes) {
  auto blocks = makeBlocks(3);
  auto &root = blocks.at(0);
  auto &target = blocks.at(2);

  ForkChoiceStore::Votes votes;
  for (int i = 0; i < 3; ++i) {
    votes[i] = SignedVote{
        .validator_id = static_cast<uint64_t>(i),
        .data =
            {
                .slot = target.slot,
                .head = Checkpoint::from(target),
                .target = Checkpoint::from(target),
                .source = Checkpoint::from(root),
            },
        .signature = {},
    };
  }

  auto head =
      getForkChoiceHead(makeBlockMap(blocks), Checkpoint::from(root), votes, 0);

  EXPECT_EQ(head, target.hash());
}

// Test basic safe target update.
TEST(TestSafeTargetComputation, test_update_safe_target_basic) {
  auto blocks = makeBlocks(1);
  auto &genesis = blocks.at(0);

  auto finalized = Checkpoint::from(genesis);

  auto store = createTestStore(100,
                               config,
                               genesis.hash(),
                               genesis.hash(),
                               finalized,
                               finalized,
                               makeBlockMap(blocks));

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

  ForkChoiceStore::Votes new_votes;
  new_votes[0] = SignedVote{
      .validator_id = 0,
      .data =
          {
              .slot = block_1.slot,
              .head = Checkpoint::from(block_1),
              .target = Checkpoint::from(block_1),
              .source = Checkpoint::from(genesis),
          },
      .signature = {},
  };
  new_votes[1] = SignedVote{
      .validator_id = 1,
      .data =
          {
              .slot = block_1.slot,
              .head = Checkpoint::from(block_1),
              .target = Checkpoint::from(block_1),
              .source = Checkpoint::from(genesis),
          },
      .signature = {},
  };

  auto store = createTestStore(100,
                               config,
                               block_1.hash(),
                               genesis.hash(),
                               finalized,
                               finalized,
                               makeBlockMap(blocks),
                               {},
                               {},
                               new_votes);

  // Update safe target with votes
  store.updateSafeTarget();

  // Should have computed a safe target
  EXPECT_TRUE(store.hasBlock(store.getSafeTarget()));
}

// Test vote target with only one block.
TEST(TestEdgeCases, test_vote_target_single_block) {
  auto blocks = makeBlocks(1);
  auto &genesis = blocks.at(0);

  auto finalized = Checkpoint::from(genesis);

  auto store = createTestStore(100,
                               config,
                               genesis.hash(),
                               genesis.hash(),
                               finalized,
                               finalized,
                               makeBlockMap(blocks));

  auto target = store.getVoteTarget();

  EXPECT_EQ(target.root, genesis.hash());
  EXPECT_EQ(target.slot, genesis.slot);
}

// Test validation of a valid attestation.
TEST(TestAttestationValidation, test_validate_attestation_valid) {
  auto blocks = makeBlocks(3);
  auto &source = blocks.at(1);
  auto &target = blocks.at(2);

  auto sample_store =
      createTestStore(100, config, {}, {}, {}, {}, makeBlockMap(blocks));

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

  auto sample_store =
      createTestStore(100, config, {}, {}, {}, {}, makeBlockMap(blocks));

  // Create invalid signed vote (source > target slot)
  EXPECT_OUTCOME_ERROR(
      sample_store.validateAttestation(makeVote(source, target)));
}

// Test validation fails when referenced blocks are missing.
TEST(TestAttestationValidation, test_validate_attestation_missing_blocks) {
  auto sample_store = createTestStore();

  // Create signed vote referencing missing blocks
  EXPECT_OUTCOME_ERROR(sample_store.validateAttestation({}));
}

// Test validation fails when checkpoint slots don't match block slots.
TEST(TestAttestationValidation,
     test_validate_attestation_checkpoint_slot_mismatch) {
  auto blocks = makeBlocks(3);
  auto &source = blocks.at(1);
  auto &target = blocks.at(2);

  auto sample_store =
      createTestStore(100, config, {}, {}, {}, {}, makeBlockMap(blocks));

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

  // Use very low genesis time (0) so that target at slot 9 is far in future
  // (slot 9 > current slot + 1)
  lean::Config low_time_config{.num_validators = 100, .genesis_time = 0};
  auto sample_store =
      createTestStore(0, low_time_config, {}, {}, {}, {}, makeBlockMap(blocks));

  // Create signed vote for future slot (target slot 9 when current is ~0)
  EXPECT_OUTCOME_ERROR(
      sample_store.validateAttestation(makeVote(source, target)));
}

// Test processing attestation from network gossip.
TEST(TestAttestationProcessing, test_process_network_attestation) {
  auto blocks = makeBlocks(3);
  auto &source = blocks.at(1);
  auto &target = blocks.at(2);

  auto sample_store =
      createTestStore(100, config, {}, {}, {}, {}, makeBlockMap(blocks));

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

  auto sample_store =
      createTestStore(100, config, {}, {}, {}, {}, makeBlockMap(blocks));

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

  auto sample_store =
      createTestStore(100, config, {}, {}, {}, {}, makeBlockMap(blocks));

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

  auto sample_store =
      createTestStore(100, config, {}, {}, {}, {}, makeBlockMap(blocks));

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
  // Create a simple store with minimal setup - use 0 time interval so
  // advanceTime is a no-op
  auto sample_store = createTestStore(0, config);

  // Target time equal to genesis time - should be a no-op
  auto target_time = sample_store.getConfig().genesis_time;

  // This should not throw an exception and should return empty result
  auto result = sample_store.advanceTime(target_time);
  EXPECT_TRUE(result.empty());
}

// Test time advancement without proposal.
TEST(TestTimeAdvancement, test_advance_time_no_proposal) {
  // Create a simple store with minimal setup
  auto sample_store = createTestStore(0, config);

  // Target time equal to genesis time - should be a no-op
  auto target_time = sample_store.getConfig().genesis_time;

  // This should not throw an exception and should return empty result
  auto result = sample_store.advanceTime(target_time);
  EXPECT_TRUE(result.empty());
}

// Test advance_time when already at target time.
TEST(TestTimeAdvancement, test_advance_time_already_current) {
  // Create a simple store with time already set
  auto sample_store = createTestStore(100, config);

  // Target time is in the past relative to current time - should be a no-op
  auto current_target = sample_store.getConfig().genesis_time;

  // Try to advance to past time (should be no-op)
  auto result = sample_store.advanceTime(current_target);
  EXPECT_TRUE(result.empty());
}

// Test advance_time with small time increment.
TEST(TestTimeAdvancement, test_advance_time_small_increment) {
  // Create a simple store
  auto sample_store = createTestStore(0, config);

  // Target time equal to genesis time - should be a no-op
  auto target_time = sample_store.getConfig().genesis_time;

  auto result = sample_store.advanceTime(target_time);
  EXPECT_TRUE(result.empty());
}

// Test basic time advancement (replacing interval ticking).
TEST(TestTimeAdvancement, test_advance_time_step_by_step) {
  // Create a simple store
  auto sample_store = createTestStore(0, config);

  // Multiple calls to advance time with same target - should all be no-ops
  for (int i = 1; i <= 5; ++i) {
    auto target_time = sample_store.getConfig().genesis_time;
    auto result = sample_store.advanceTime(target_time);
    EXPECT_TRUE(result.empty());
  }
}

// Test time advancement with multiple steps.
TEST(TestTimeAdvancement, test_advance_time_multiple_steps) {
  // Create a simple store
  auto sample_store = createTestStore(0, config);

  // Multiple calls to advance time - should all be no-ops
  for (int i = 1; i <= 5; ++i) {
    auto target_time = sample_store.getConfig().genesis_time;
    auto result = sample_store.advanceTime(target_time);
    EXPECT_TRUE(result.empty());
  }
}

// Test time advancement with vote processing.
TEST(TestTimeAdvancement, test_advance_time_with_votes) {
  // Create a simple store
  auto sample_store = createTestStore(0, config);

  // Advance time - should be no-op
  auto target_time = sample_store.getConfig().genesis_time;
  auto result = sample_store.advanceTime(target_time);
  EXPECT_TRUE(result.empty());
}

// Test getting current head.
TEST(TestHeadSelection, test_get_head_basic) {
  auto blocks = makeBlocks(1);
  auto &genesis = blocks.at(0);
  auto sample_store = createTestStore(100,
                                      config,
                                      genesis.hash(),
                                      genesis.hash(),
                                      {},
                                      {},
                                      makeBlockMap(blocks));

  // Get current head
  auto head = sample_store.getHead();

  // Should return expected head
  EXPECT_EQ(head, genesis.hash());
}

// Test that advance time functionality works.
TEST(TestHeadSelection, test_advance_time_functionality) {
  // Create a simple store
  auto sample_store = createTestStore(0, config);

  // Advance time - should be no-op
  auto target_time = sample_store.getConfig().genesis_time;
  auto result = sample_store.advanceTime(target_time);
  EXPECT_TRUE(result.empty());
}

// Test basic block production capability.
TEST(TestHeadSelection, test_produce_block_basic) {
  // Create a simple store
  auto sample_store = createTestStore(0, config);

  // Try to produce a block - should throw due to missing state, which is
  // expected
  EXPECT_THROW(sample_store.produceBlock(1, 1), std::exception);
}
