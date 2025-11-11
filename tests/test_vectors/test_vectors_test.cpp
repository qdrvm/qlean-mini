/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <filesystem>

#include <qtils/read_file.hpp>

#include "blockchain/state_transition_function.hpp"
#include "mock/metrics_mock.hpp"
#include "serde/json.hpp"
#include "types/state_transition_test_json.hpp"

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

using Param = std::pair<std::string, lean::StateTransitionTestJson>;

struct StateTransitionTest : testing::TestWithParam<Param> {};

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

INSTANTIATE_TEST_SUITE_P(
    StateTransition,
    StateTransitionTest,
    testing::ValuesIn([] {
      std::vector<Param> params;
      auto search_dir =
          std::filesystem::path{PROJECT_SOURCE_DIR}
          / "tests/test_vectors/fixtures/consensus/state_transition";
      std::error_code ec;
      for (auto &item :
           std::filesystem::recursive_directory_iterator{search_dir, ec}) {
        auto &json_path = item.path();
        if (json_path.extension() != ".json") {
          continue;
        }
        auto json_str = qtils::readText(json_path).value();
        std::unordered_map<std::string, lean::StateTransitionTestJson> fixtures;
        lean::json::decode(fixtures, json_str);
        for (auto &[name, fixture] : fixtures) {
          params.emplace_back(name, fixture);
        }
      }
      return params;
    }()),
    [](auto &&info) { return escapeTestLabel(info.param.first); });
