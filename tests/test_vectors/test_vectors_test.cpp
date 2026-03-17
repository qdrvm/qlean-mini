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
#include "types/ssz_test_json.hpp"

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
