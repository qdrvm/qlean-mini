/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <gtest/gtest.h>

#include <filesystem>
#include <print>
#include <unordered_map>

#include <qtils/read_file.hpp>

#include "serde/json.hpp"

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

inline auto escapeTestLabel(std::string_view label) {
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
