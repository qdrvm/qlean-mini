/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/fork_choice.hpp"

#include "blockchain/impl/anchor_block_impl.hpp"
#include "blockchain/impl/anchor_state_impl.hpp"
#include "fork_choice_test_json.hpp"
#include "mock/app/chain_spec_mock.hpp"
#include "mock/app/configuration_mock.hpp"
#include "mock/app/validator_keys_manifest_mock.hpp"
#include "mock/blockchain/block_storage_mock.hpp"
#include "mock/blockchain/block_tree_mock.hpp"
#include "mock/blockchain/validator_registry_mock.hpp"
#include "mock/clock/manual_clock.hpp"
#include "mock/crypto/xmss_provider_mock.hpp"
#include "mock/metrics_mock.hpp"
#include "test_vectors.hpp"
#include "testutil/prepare_loggers.hpp"

using lean::BlockHash;
using testing::_;

struct ForkChoiceTest : FixtureTest<lean::ForkChoiceTestJson> {};
FIXTURE_INSTANTIATE(ForkChoiceTest, "fork_choice");

TEST_P(ForkChoiceTest, ForkChoice) {
  auto &[name, fixture] = GetParam();
  std::println("RUN {}", name);
  // in last proposer attestation, target < source
  if (name == "tests/consensus/devnet/fc/test_fork_choice_reorgs.py::test_reorg_on_newly_justified_slot[fork_Devnet][fork_devnet-fork_choice_test]"
    or name == "tests/consensus/devnet/fc/test_signature_aggregation.py::test_all_validators_attest_in_single_aggregation[fork_Devnet][fork_devnet-fork_choice_test]"
    or name == "tests/consensus/devnet/fc/test_signature_aggregation.py::test_multiple_specs_same_target_merge_into_one[fork_Devnet][fork_devnet-fork_choice_test]"
    or name == "tests/consensus/devnet/fc/test_finalization_mid_processing.py::test_finalization_advances_mid_attestation_processing[fork_Devnet][fork_devnet-fork_choice_test]"
  ) {
    std::println("  DISABLED");
    return;
  }

  auto anchor_state =
      std::make_shared<lean::blockchain::AnchorStateImpl>(fixture.anchor_state);
  auto anchor_block =
      std::make_shared<lean::blockchain::AnchorBlockImpl>(*anchor_state);
  fixture.anchor_block.setHash();
  EXPECT_EQ(anchor_block->hash(), fixture.anchor_block.hash());

  auto logsys = testutil::prepareLoggers();

  auto clock = std::make_shared<lean::clock::ManualClock>();

  auto app_config = std::make_shared<lean::app::ConfigurationMock>();
  EXPECT_CALL(*app_config, cliSubnetCount()).WillOnce(testing::Return(1));

  lean::ValidatorRegistry::ValidatorIndices validator_indices{0};
  auto validator_registry = std::make_shared<lean::ValidatorRegistryMock>();
  EXPECT_CALL(*validator_registry, currentValidatorIndices())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::ReturnRef(validator_indices));
  EXPECT_CALL(*validator_registry, nodeIdByIndex(_))
      .WillRepeatedly(
          [](lean::ValidatorIndex i) { return std::format("node_{}", i); });

  auto chain_spec = std::make_shared<lean::app::ChainSpecMock>();
  EXPECT_CALL(*chain_spec, isAggregator()).WillOnce(testing::Return(true));

  auto validator_key_manifest =
      std::make_shared<lean::app::ValidatorKeysManifestMock>();
  EXPECT_CALL(*validator_key_manifest, getAllXmssPubkeys())
      .Times(testing::AnyNumber());
  EXPECT_CALL(*validator_key_manifest, getKeypair(_))
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(std::nullopt));

  auto xmss = std::make_shared<lean::crypto::xmss::XmssProviderMock>();
  EXPECT_CALL(*xmss, verify(_, _, _, _)).WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*xmss, sign(_, _, _)).Times(testing::AnyNumber());
  EXPECT_CALL(*xmss, verifyAggregatedSignatures(_, _, _, _))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*xmss, aggregateSignatures(_, _, _, _, _, _))
      .Times(testing::AnyNumber());

  auto block_tree = std::make_shared<lean::blockchain::BlockTreeMock>();
  auto last_finalized = fixture.anchor_block.index();
  auto last_justified = last_finalized;
  std::unordered_map<BlockHash, lean::BlockHeader> blocks;
  std::unordered_map<BlockHash, std::vector<BlockHash>> children;
  blocks.emplace(fixture.anchor_block.hash(), fixture.anchor_block.getHeader());
  EXPECT_CALL(*block_tree, lastFinalized()).WillRepeatedly([&] {
    return last_finalized;
  });
  EXPECT_CALL(*block_tree, finalize(_)).WillRepeatedly([&](BlockHash hash) {
    last_finalized = blocks.at(hash).index();
    return outcome::success();
  });
  EXPECT_CALL(*block_tree, getLatestJustified()).WillRepeatedly([&] {
    return last_justified;
  });
  EXPECT_CALL(*block_tree, setJustified(_)).WillRepeatedly([&](BlockHash hash) {
    last_justified = blocks.at(hash).index();
    return outcome::success();
  });
  EXPECT_CALL(*block_tree, has(_)).WillRepeatedly([&](BlockHash hash) {
    return blocks.contains(hash);
  });
  EXPECT_CALL(*block_tree, getSlotByHash(_))
      .WillRepeatedly([&](BlockHash hash) { return blocks.at(hash).slot; });
  EXPECT_CALL(*block_tree, getBlockHeader(_))
      .WillRepeatedly([&](BlockHash hash) { return blocks.at(hash); });
  EXPECT_CALL(*block_tree, tryGetBlockHeader(_))
      .WillRepeatedly([&](BlockHash hash) { return blocks.at(hash); });
  EXPECT_CALL(*block_tree, getChildren(_)).WillRepeatedly([&](BlockHash hash) {
    return children[hash];
  });
  EXPECT_CALL(*block_tree, addBlock(_))
      .WillRepeatedly([&](lean::SignedBlock block) {
        auto header = block.block.getHeader();
        header.updateHash();
        blocks.emplace(header.hash(), header);
        children[header.parent_root].emplace_back(header.hash());
        return outcome::success();
      });

  auto block_storage = std::make_shared<lean::blockchain::BlockStorageMock>();
  std::unordered_map<BlockHash, lean::State> states;
  states.emplace(fixture.anchor_block.hash(), fixture.anchor_state);
  EXPECT_CALL(*block_storage, getState(_)).WillRepeatedly([&](BlockHash hash) {
    return states.at(hash);
  });
  EXPECT_CALL(*block_storage, putState(_, _))
      .WillRepeatedly([&](BlockHash hash, lean::State state) {
        states.emplace(hash, state);
        return outcome::success();
      });

  lean::ForkChoiceStore store{
      anchor_state,
      anchor_block,
      clock,
      logsys,
      std::make_shared<lean::metrics::MetricsMock>(),
      app_config,
      validator_registry,
      chain_spec,
      validator_key_manifest,
      xmss,
      block_tree,
      block_storage,
  };
  store.dontPropose();
  auto check = [&](const lean::BaseForkChoiceStep &step, auto &&f) {
    outcome::result<void> r = f();
    ASSERT_EQ(step.valid, r.has_value());
    if (auto &checks = step.checks) {
      if (checks->time) {
        ASSERT_EQ(store.time().interval, *checks->time);
      }
      if (checks->head_slot) {
        ASSERT_EQ(store.getHead().slot, *checks->head_slot);
      }
      if (checks->head_root) {
        ASSERT_EQ(store.getHead().root, *checks->head_root);
      }
      if (checks->latest_justified_slot) {
        ASSERT_EQ(store.getLatestJustified().slot,
                  *checks->latest_justified_slot);
      }
      if (checks->latest_justified_root) {
        ASSERT_EQ(store.getLatestJustified().root,
                  *checks->latest_justified_root);
      }
      if (checks->latest_finalized_slot) {
        ASSERT_EQ(store.getLatestFinalized().slot,
                  *checks->latest_finalized_slot);
      }
      if (checks->latest_finalized_root) {
        ASSERT_EQ(store.getLatestFinalized().root,
                  *checks->latest_finalized_root);
      }
      if (checks->safe_target) {
        ASSERT_EQ(store.getSafeTarget().root, *checks->safe_target);
      }
      if (checks->attestation_target_slot) {
        auto target = store.getAttestationTarget(
            store.getLatestJustified(), store.getHead(), std::nullopt);
        ASSERT_EQ(target.slot, *checks->attestation_target_slot);
        ASSERT_EQ(store.getBlockSlot(target.root).value(), target.slot);
      }
      if (checks->attestation_checks) {
        for (auto &check : *checks->attestation_checks) {
          auto &attestations =
              check.location == lean::AttestationCheckLocation::NEW
                  ? store.getLatestNewAttestations()
                  : store.getLatestKnownAttestations();
          auto &data = attestations.at(check.validator);
          ASSERT_EQ(data.slot, check.attestation_slot);
          if (check.head_slot) {
            ASSERT_EQ(data.head.slot, *check.head_slot);
          }
          if (check.source_slot) {
            ASSERT_EQ(data.source.slot, *check.source_slot);
          }
          if (check.target_slot) {
            ASSERT_EQ(data.target.slot, *check.target_slot);
          }
        }
      }
    }
  };
  for (auto &step : fixture.steps) {
    std::println("");
    if (auto *tick_step = std::get_if<lean::TickStep>(&step.v)) {
      std::println("STEP TICK {}", tick_step->time);
      check(*tick_step, [&]() -> outcome::result<void> {
        store.onTick(std::chrono::seconds{tick_step->time});
        return outcome::success();
      });
    } else if (auto *block_step = std::get_if<lean::BlockStep>(&step.v)) {
      std::println("STEP BLOCK {}", block_step->block.slot);
      check(*block_step, [&] {
        auto &block = block_step->block;
        lean::SignedBlock signed_block{
            .block = block,
            .signature = {},
        };
        signed_block.signature.attestation_signatures.data().resize(
            block.body.attestations.size());
        auto block_time = std::chrono::seconds{store.getConfig().genesis_time}
                        + block.slot * lean::SLOT_DURATION_MS;
        store.onTick(block_time);
        return store.onBlock(signed_block);
      });
      if (auto &checks = block_step->checks) {
        if (checks->block_attestation_count) {
          EXPECT_EQ(block_step->block.body.attestations.size(),
                    *checks->block_attestation_count);
        }
        if (checks->block_attestations) {
          auto &attestations = block_step->block.body.attestations.data();
          for (auto &check : *checks->block_attestations) {
            lean::AggregationBits participants;
            for (auto &validator : check.participants) {
              participants.add(validator);
            }
            auto attestation_it = std::ranges::find_if(
                attestations,
                [&](const lean::AggregatedAttestation &attestation) {
                  return attestation.aggregation_bits == participants;
                });
            EXPECT_NE(attestation_it, attestations.end());
            auto &attestation = *attestation_it;
            if (check.attestation_slot) {
              EXPECT_EQ(attestation.data.slot, *check.attestation_slot);
            }
            if (check.target_slot) {
              EXPECT_EQ(attestation.data.target.slot, *check.target_slot);
            }
          }
        }
      }
    } else if (auto *attestation_step =
                   std::get_if<lean::AttestationStep>(&step.v)) {
      std::println("STEP ATTESTATION {}",
                   attestation_step->attestation.data.target.slot);
      check(*attestation_step, [&] {
        return store.onGossipAttestation(attestation_step->attestation);
      });
    }
  }
}
