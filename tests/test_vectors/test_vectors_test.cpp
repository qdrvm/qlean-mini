/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <print>

#include <qtils/read_file.hpp>

#include "blockchain/fork_choice.hpp"
#include "blockchain/state_transition_function.hpp"
#include "mock/app/validator_keys_manifest_mock.hpp"
#include "mock/blockchain/validator_registry_mock.hpp"
#include "mock/clock/manual_clock.hpp"
#include "mock/crypto/xmss_provider_mock.hpp"
#include "mock/metrics_mock.hpp"
#include "serde/json.hpp"
#include "testutil/prepare_loggers.hpp"
#include "types/fork_choice_test_json.hpp"
#include "types/state_transition_test_json.hpp"

#define FIXTURE_INSTANTIATE(test_name, directory)                              \
  INSTANTIATE_TEST_SUITE_P(                                                    \
      Fixture,                                                                 \
      test_name,                                                               \
      testing::ValuesIn([] {                                                   \
        std::vector<test_name::ParamType> params;                              \
        auto search_dir = std::filesystem::path{PROJECT_SOURCE_DIR}            \
                        / "tests/test_vectors/fixtures/consensus" / directory; \
        std::error_code ec;                                                    \
        for (auto &item :                                                      \
             std::filesystem::recursive_directory_iterator{search_dir, ec}) {  \
          auto &json_path = item.path();                                       \
          if (json_path.extension() != ".json") {                              \
            continue;                                                          \
          }                                                                    \
          auto json_str = qtils::readText(json_path).value();                  \
          std::unordered_map<std::string,                                      \
                             decltype(test_name::ParamType::second)>           \
              fixtures;                                                        \
          lean::json::decode(fixtures, json_str);                              \
          for (auto &[name, fixture] : fixtures) {                             \
            params.emplace_back(name, fixture);                                \
          }                                                                    \
        }                                                                      \
        return params;                                                         \
      }()),                                                                    \
      [](auto &&info) { return escapeTestLabel(info.param.first); })

auto escapeTestLabel(std::string_view label) {
  std::string name;
  name.reserve(label.size() * 2);
  for (auto c : label) {
    if (c == '/') {
      name.append("__");
    } else if (not isalnum(c)) {
      name.push_back('_');
    } else {
      name.push_back(c);
    }
  }
  return name;
}

template <typename T>
struct FixtureTest : testing::TestWithParam<std::pair<std::string, T>> {};

struct StateTransitionTest : FixtureTest<lean::StateTransitionTestJson> {};
FIXTURE_INSTANTIATE(StateTransitionTest, "state_transition");

TEST_P(StateTransitionTest, StateTransition) {
  auto &[name, fixture] = GetParam();
  std::println("RUN {}", name);
  lean::STF stf{std::make_shared<lean::metrics::MetricsMock>()};
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
    if (post.latest_block_header_slot) {
      ASSERT_EQ(state.latest_block_header.slot, *post.latest_block_header_slot);
    }
    if (post.latest_block_header_state_root) {
      ASSERT_EQ(state.latest_block_header.state_root,
                *post.latest_block_header_state_root);
    }
    if (post.historical_block_hashes_count) {
      ASSERT_EQ(state.historical_block_hashes.size(),
                *post.historical_block_hashes_count);
    }
  }
}

struct ForkChoiceTest : FixtureTest<lean::ForkChoiceTestJson> {};
FIXTURE_INSTANTIATE(ForkChoiceTest, "fork_choice");

TEST_P(ForkChoiceTest, ForkChoice) {
  auto &[name, fixture] = GetParam();
  std::println("RUN {}", name);
  auto clock = std::make_shared<lean::clock::ManualClock>();
  lean::ValidatorRegistry::ValidatorIndices validator_indices;
  auto validator_registry = std::make_shared<lean::ValidatorRegistryMock>();
  EXPECT_CALL(*validator_registry, currentValidatorIndices())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::ReturnRef(validator_indices));
  lean::ForkChoiceStore store{
      fixture.anchor_state,
      fixture.anchor_block,
      clock,
      testutil::prepareLoggers(),
      std::make_shared<lean::metrics::MetricsMock>(),
      validator_registry,
      std::make_shared<lean::app::ValidatorKeysManifestMock>(),
      std::make_shared<lean::crypto::xmss::XmssProviderMock>(),
  };
  auto check = [&](const lean::BaseForkChoiceStep &step, auto &&f) {
    outcome::result<void> r = f();
    ASSERT_EQ(step.valid, r.has_value());
    if (auto &checks = step.checks) {
      if (checks->time) {
        ASSERT_EQ(store.time(), *checks->time);
      }
      if (checks->head_slot) {
        ASSERT_EQ(store.getHeadSlot(), *checks->head_slot);
      }
      if (checks->head_root) {
        ASSERT_EQ(store.getHead(), *checks->head_root);
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
        ASSERT_EQ(store.getSafeTarget(), *checks->safe_target);
      }
      if (checks->attestation_target_slot) {
        auto target = store.getAttestationTarget();
        ASSERT_EQ(target.slot, *checks->attestation_target_slot);
        ASSERT_EQ(store.getBlockSlot(target.root), target.slot);
      }
      if (checks->attestation_checks) {
        for (auto &check : *checks->attestation_checks) {
          auto &attestations =
              check.location == lean::AttestationCheckLocation::NEW
                  ? store.getLatestNewAttestations()
                  : store.getLatestKnownAttestations();
          auto &data = attestations.at(check.validator).message.data;
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
    if (auto *tick_step = std::get_if<lean::TickStep>(&step.v)) {
      check(*tick_step, [&]() -> outcome::result<void> {
        store.onTick(tick_step->time);
        return outcome::success();
      });
    } else if (auto *block_step = std::get_if<lean::BlockStep>(&step.v)) {
      check(*block_step, [&] {
        auto &block_with_attestation = block_step->block;
        auto &block = block_with_attestation.block;
        lean::SignedBlockWithAttestation signed_block_with_attestation{
            .message = block_with_attestation,
            .signature = {},
        };
        signed_block_with_attestation.signature.data().resize(
            block.body.attestations.size() + 1);
        auto block_time = store.getConfig().genesis_time
                        + block.slot * lean::SECONDS_PER_SLOT;
        store.onTick(block_time);
        return store.onBlock(signed_block_with_attestation);
      });
    } else if (auto *attestation_step =
                   std::get_if<lean::AttestationStep>(&step.v)) {
      check(*attestation_step, [&] {
        return store.onAttestation(attestation_step->attestation, false);
      });
    }
  }
}
