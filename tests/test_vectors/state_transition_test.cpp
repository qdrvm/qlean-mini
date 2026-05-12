/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/state_transition_function.hpp"
#include "mock/blockchain/block_tree_mock.hpp"
#include "mock/metrics_mock.hpp"
#include "state_transition_test_json.hpp"
#include "test_vectors.hpp"
#include "testutil/prepare_loggers.hpp"

struct StateTransitionTest : FixtureTest<lean::StateTransitionTestJson> {};
FIXTURE_INSTANTIATE(StateTransitionTest, "state_transition");

TEST_P(StateTransitionTest, StateTransition) {
  auto &[name, fixture] = GetParam();
  std::println("RUN {}", name);
  auto logsys = testutil::prepareLoggers();
  auto block_tree = std::make_shared<lean::blockchain::BlockTreeMock>();
  EXPECT_CALL(*block_tree, getLatestJustified()).Times(testing::AnyNumber());
  EXPECT_CALL(*block_tree, lastFinalized()).Times(testing::AnyNumber());
  lean::STF stf{
      logsys,
      block_tree,
      std::make_shared<lean::metrics::MetricsMock>(),
  };
  auto stf_many = [&]() -> outcome::result<lean::State> {
    auto state = fixture.pre;
    for (auto &block : fixture.blocks) {
      BOOST_OUTCOME_TRY(state, stf.stateTransition(block, state, true));
    }
    return state;
  };
  auto state_result = stf_many();
  ASSERT_EQ(state_result.has_value(), fixture.post.has_value());
  ASSERT_EQ(state_result.has_value(), not fixture.expect_exception.has_value());
  if (state_result.has_value()) {
    auto &state = state_result.value();
    auto &post = *fixture.post;
    ASSERT_EQ(state.slot, post.slot);
    if (post.latest_justified_slot) {
      ASSERT_EQ(state.latest_justified.slot, *post.latest_justified_slot);
    }
    if (post.latest_justified_root) {
      ASSERT_EQ(state.latest_justified.root, *post.latest_justified_root);
    }
    if (post.latest_finalized_slot) {
      ASSERT_EQ(state.latest_finalized.slot, *post.latest_finalized_slot);
    }
    if (post.latest_finalized_root) {
      ASSERT_EQ(state.latest_finalized.root, *post.latest_finalized_root);
    }
    if (post.validator_count) {
      ASSERT_EQ(state.validators.size(), *post.validator_count);
    }
    if (post.config_genesis_time) {
      ASSERT_EQ(state.config.genesis_time, *post.config_genesis_time);
    }
    if (post.latest_block_header_slot) {
      ASSERT_EQ(state.latest_block_header.slot, *post.latest_block_header_slot);
    }
    if (post.latest_block_header_proposer_index) {
      ASSERT_EQ(state.latest_block_header.proposer_index,
                *post.latest_block_header_proposer_index);
    }
    if (post.latest_block_header_parent_root) {
      ASSERT_EQ(state.latest_block_header.parent_root,
                *post.latest_block_header_parent_root);
    }
    if (post.latest_block_header_state_root) {
      ASSERT_EQ(state.latest_block_header.state_root,
                *post.latest_block_header_state_root);
    }
    if (post.latest_block_header_body_root) {
      ASSERT_EQ(state.latest_block_header.body_root,
                *post.latest_block_header_body_root);
    }
    if (post.historical_block_hashes_count) {
      ASSERT_EQ(state.historical_block_hashes.size(),
                *post.historical_block_hashes_count);
    }
    if (post.historical_block_hashes) {
      ASSERT_EQ(state.historical_block_hashes, *post.historical_block_hashes);
    }
    if (post.justified_slots) {
      ASSERT_EQ(state.justified_slots, *post.justified_slots);
    }
    if (post.justifications_roots) {
      ASSERT_EQ(state.justifications_roots, *post.justifications_roots);
    }
    if (post.justifications_validators) {
      ASSERT_EQ(state.justifications_validators,
                *post.justifications_validators);
    }
  }
}
