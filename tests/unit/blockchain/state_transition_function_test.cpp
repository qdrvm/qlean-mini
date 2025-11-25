/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/state_transition_function.hpp"

#include <gtest/gtest.h>

#include "mock/app/validator_keys_manifest_mock.hpp"
#include "mock/blockchain/metrics_mock.hpp"
#include "mock/blockchain/validator_registry_mock.hpp"
#include "types/config.hpp"
#include "types/state.hpp"

TEST(STF, Test) {
  auto metrics = std::make_shared<lean::metrics::MetricsMock>();
  lean::STF stf(metrics);

  lean::Config config{
      .genesis_time = 0,
  };
  auto validator_registry = std::make_shared<lean::ValidatorRegistryMock>();
  // Create a validator set with at least 2 validators for the test
  lean::ValidatorRegistry::ValidatorIndices validators = {0, 1};
  EXPECT_CALL(*validator_registry, allValidatorsIndices())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(validators));

  auto validator_keys_manifest =
      std::make_shared<lean::app::ValidatorKeysManifestMock>();
  // Return a dummy public key for validators 0 and 1
  EXPECT_CALL(*validator_keys_manifest, getXmssPubkeyByIndex(testing::_))
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Invoke([](lean::ValidatorIndex idx) {
        if (idx < 2) {
          lean::crypto::xmss::XmssPublicKey pubkey;
          std::fill(pubkey.begin(), pubkey.end(), static_cast<uint8_t>(idx));
          return std::make_optional(pubkey);
        }
        return std::optional<lean::crypto::xmss::XmssPublicKey>{};
      }));

  auto state0 = lean::STF::generateGenesisState(
      config, validator_registry, validator_keys_manifest);
  auto block0 = lean::STF::genesisBlock(state0);
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
      .proposer_index = (block1.slot + 3) % 2,
      .parent_root = block1.hash(),
  };
  auto state2 = stf.stateTransition(block2, state1, false).value();
  block2.state_root = lean::sszHash(state2);
  auto state2_apply = stf.stateTransition(block2, state1, true).value();
  EXPECT_EQ(state2_apply, state2);
  block2.setHash();
}
