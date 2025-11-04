/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/state_transition_function.hpp"

#include <gtest/gtest.h>

#include "types/config.hpp"
#include "types/state.hpp"

TEST(STF, Test) {
  lean::STF stf;

  lean::Config config{
      .num_validators = 2,
      .genesis_time = 0,
  };
  auto state0 = stf.generateGenesisState(config);
  auto block0 = stf.genesisBlock(state0);
  block0.setHash();

  lean::Block block1{
      .slot = block0.slot + 1,
      .proposer_index = 1,
      .parent_root = block0.hash(),
  };
  auto state1 = stf.stateTransition(block1, state0, false).value();
  block1.state_root = lean::sszHash(state1);
  auto state1_apply = stf.stateTransition(block1, state0, true).value();
  EXPECT_EQ(state1_apply, state1);
  block1.setHash();

  lean::Block block2{
      .slot = block1.slot + 3,
      .parent_root = block1.hash(),
  };
  auto state2 = stf.stateTransition(block2, state1, false).value();
  block2.state_root = lean::sszHash(state2);
  auto state2_apply = stf.stateTransition(block2, state1, true).value();
  EXPECT_EQ(state2_apply, state2);
  block2.setHash();
}
