/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "blockchain/impl/validator_registry_impl.hpp"
#include "testutil/prepare_loggers.hpp"

TEST(ValidatorRegistryTest, LoadsRegistryAndResolvesIndices) {
  auto logger = testutil::prepareLoggers();

  lean::ValidatorRegistryImpl registry{
      logger,
      R"(
node_0:
  - 0
node_1:
  - 1
)",
      "node_1",
  };

  EXPECT_EQ(registry.currentValidatorIndices(),
            lean::ValidatorRegistry::ValidatorIndices{1});
  EXPECT_EQ(registry.nodeIdByIndex(0), std::optional<std::string>{"node_0"});
  EXPECT_EQ(registry.nodeIdByIndex(1), std::optional<std::string>{"node_1"});
}

TEST(ValidatorRegistryTest, MissingNodeDefaultsToZero) {
  auto logger = testutil::prepareLoggers();

  lean::ValidatorRegistryImpl registry{
      logger,
      R"(
node_0:
  - 0
)",
      "unknown_node",
  };

  EXPECT_EQ(registry.currentValidatorIndices(),
            lean::ValidatorRegistry::ValidatorIndices{});
}

TEST(ValidatorRegistryTest, ThrowsOnDuplicateIndex) {
  auto logger = testutil::prepareLoggers();

  EXPECT_THROW((lean::ValidatorRegistryImpl{
                   logger,
                   R"(
node_0:
  - 0
node_1:
  - 0
)",
                   "node_0",
               }),
               std::runtime_error);
}
