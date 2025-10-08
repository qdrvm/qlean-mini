/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "utils/validator_registry.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "tests/testutil/prepare_loggers.hpp"

namespace {

  std::filesystem::path createTempRegistryFile(const std::string &content) {
    auto temp_dir = std::filesystem::temp_directory_path();
    auto timestamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    auto temp_file =
        temp_dir
        / std::filesystem::path("validator-registry-"
                                + std::to_string(timestamp) + ".yaml");
    std::ofstream ofs(temp_file);
    ofs << content;
    ofs.close();
    return temp_file;
  }

}  // namespace

TEST(ValidatorRegistryTest, LoadsRegistryAndResolvesIndices) {
  const auto content = R"(node_0:
  - 0
node_1:
  - 1
)";
  auto path = createTempRegistryFile(content);
  auto logger = testutil::prepareLoggers();

  auto registry =
      lean::ValidatorRegistry::createForTesting(logger, path, "node_1");

  EXPECT_EQ(registry.currentValidatorIndices(),
            lean::ValidatorRegistry::ValidatorIndices{1});
  EXPECT_EQ(registry.nodeIdByIndex(0), std::optional<std::string>{"node_0"});
  EXPECT_EQ(registry.nodeIdByIndex(1), std::optional<std::string>{"node_1"});
  std::filesystem::remove(path);
}

TEST(ValidatorRegistryTest, MissingNodeDefaultsToZero) {
  const auto content = R"(node_0:
  - 0
)";
  auto path = createTempRegistryFile(content);
  auto logger = testutil::prepareLoggers();

  auto registry =
      lean::ValidatorRegistry::createForTesting(logger, path, "unknown_node");

  EXPECT_EQ(registry.currentValidatorIndices(),
            lean::ValidatorRegistry::ValidatorIndices{});
  std::filesystem::remove(path);
}

TEST(ValidatorRegistryTest, ThrowsOnDuplicateIndex) {
  const auto content = R"(node_0:
  - 0
node_1:
  - 0
)";
  auto path = createTempRegistryFile(content);
  auto logger = testutil::prepareLoggers();

  EXPECT_THROW(
      lean::ValidatorRegistry::createForTesting(logger, path, "node_0"),
      std::runtime_error);
  std::filesystem::remove(path);
}
