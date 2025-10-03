/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>
#include <fstream>

#include "serde/enr.hpp"
#include "utils/sample_peer.hpp"

inline void cmdGenerateGenesis(const std::filesystem::path &genesis_directory,
                               size_t validator_count) {
  uint64_t genesis_time =
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();

  std::filesystem::create_directories(genesis_directory);

  std::ofstream config_yaml{genesis_directory / "config.yaml"};
  std::println(config_yaml, "GENESIS_TIME: {}", genesis_time);
  std::println(config_yaml, "VALIDATOR_COUNT: {}", validator_count);
  config_yaml.close();

  std::ofstream nodes_yaml{genesis_directory / "nodes.yaml"};
  for (size_t i = 0; i < validator_count; ++i) {
    lean::SamplePeer peer{i};
    std::println(nodes_yaml, "- {}", peer.enr());

    std::ofstream node_key_hex{genesis_directory
                               / std::format("node_key_{}.hex", i)};
    std::println(
        node_key_hex,
        "{}",
        fmt::format("{:0xx}", qtils::Hex{peer.keypair.privateKey.data}));
    node_key_hex.close();
  }
  nodes_yaml.close();

  std::ofstream validators_yaml{genesis_directory / "validators.yaml"};
  for (size_t i = 0; i < validator_count; ++i) {
    std::println(validators_yaml, "node_{}:", i);
    std::println(validators_yaml, "    - {}", i);
  }
  validators_yaml.close();
}
