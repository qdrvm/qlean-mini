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
#include "crypto/xmss/xmss_provider_impl.hpp"
#include "mock/app/validator_keys_manifest_mock.hpp"
#include "mock/blockchain/block_storage_mock.hpp"
#include "mock/blockchain/block_tree_mock.hpp"
#include "mock/blockchain/validator_registry_mock.hpp"
#include "mock/clock/manual_clock.hpp"
#include "mock/crypto/xmss_provider_mock.hpp"
#include "mock/metrics_mock.hpp"
#include "serde/json.hpp"
#include "ssz_test_json.hpp"
#include "state_transition_test_json.hpp"
#include "testutil/prepare_loggers.hpp"
#include "verify_signatures_test_json.hpp"

using testing::_;

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
          lean::json::decode(lean::json::NameCase::CAMEL, fixtures, json_str); \
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

struct SszTest : FixtureTest<lean::SszTestJson> {};
FIXTURE_INSTANTIATE(SszTest, "ssz");

TEST_P(SszTest, Ssz) {
  auto &[name, fixture] = GetParam();
  std::println("RUN {}", name);
  std::println("  TYPE {}", fixture.typeName());
  if (fixture.disabled()) {
    std::println("  DISABLED");
    return;
  }
  auto [actual, expected] = std::visit(
      [&](auto &v) {
        return std::make_pair(lean::encode(v.value).value(), v.serialized);
      },
      fixture.v);
  EXPECT_EQ(actual.toHex(), expected.toHex());
}

struct VerifySignaturesTest : FixtureTest<lean::VerifySignaturesTestJson> {};
FIXTURE_INSTANTIATE(VerifySignaturesTest, "verify_signatures");

TEST_P(VerifySignaturesTest, VerifySignatures) {
  auto &[name, fixture] = GetParam();
  std::println("RUN {}", name);
  auto logsys = testutil::prepareLoggers();
  lean::ValidatorRegistry::ValidatorIndices validator_indices{0};
  auto validator_registry = std::make_shared<lean::ValidatorRegistryMock>();
  EXPECT_CALL(*validator_registry, currentValidatorIndices())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::ReturnRef(validator_indices));
  auto block_storage = std::make_shared<lean::blockchain::BlockStorageMock>();
  EXPECT_CALL(
      *block_storage,
      getState(fixture.signed_block_with_attestation.message.block.parent_root))
      .WillOnce(testing::Return(fixture.anchor_state));
  lean::ForkChoiceStore store{
      {},
      logsys,
      std::make_shared<lean::metrics::MetricsMock>(),
      {},
      {},
      {},
      {},
      {},
      0,
      validator_registry,
      std::make_shared<lean::app::ValidatorKeysManifestMock>(),
      std::make_shared<lean::crypto::xmss::XmssProviderImpl>(),
      std::make_shared<lean::blockchain::BlockTreeMock>(),
      block_storage,
      false,
      1,
  };
  EXPECT_EQ(
      store.validateBlockSignatures(fixture.signed_block_with_attestation),
      not fixture.expect_exception.has_value());
}

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
