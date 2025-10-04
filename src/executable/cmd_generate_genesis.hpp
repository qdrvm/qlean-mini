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

  std::vector<lean::SamplePeer> peers;
  for (size_t i = 0; i < validator_count; ++i) {
    peers.emplace_back(lean::SamplePeer{i});
  }

  std::ofstream config_yaml{genesis_directory / "config.yaml"};
  std::println(config_yaml, "GENESIS_TIME: {}", genesis_time);
  std::println(config_yaml, "VALIDATOR_COUNT: {}", validator_count);
  config_yaml.close();

  std::ofstream validator_config_yaml{genesis_directory
                                      / "validator-config.yaml"};
  std::println(validator_config_yaml, "shuffle: roundrobin");
  std::println(validator_config_yaml, "validators:");
  for (size_t i = 0; i < validator_count; ++i) {
    std::println(validator_config_yaml, "  - name: \"node_{}\"", i);
    std::println(validator_config_yaml,
                 "    privkey: \"{}\"",
                 qtils::ByteView{peers.at(i).keypair.privateKey.data}.toHex());
    std::println(validator_config_yaml, "    enrFields:");
    std::println(validator_config_yaml, "      ip: \"127.0.0.1\"");
    std::println(validator_config_yaml, "      quic: {}", peers.at(i).port);
    std::println(validator_config_yaml, "    count: 1");
  }
  validator_config_yaml.close();

  std::ofstream nodes_yaml{genesis_directory / "nodes.yaml"};
  for (size_t i = 0; i < validator_count; ++i) {
    std::println(nodes_yaml, "- {}", peers.at(i).enr());
  }
  nodes_yaml.close();

  for (size_t i = 0; i < validator_count; ++i) {
    std::ofstream node_key_hex{genesis_directory
                               / std::format("node_{}.key", i)};
    std::print(node_key_hex,
               "{}",
               qtils::ByteView{peers.at(i).keypair.privateKey.data}.toHex());
    node_key_hex.close();
  }

  std::ofstream validators_yaml{genesis_directory / "validators.yaml"};
  for (size_t i = 0; i < validator_count; ++i) {
    std::println(validators_yaml, "node_{}:", i);
    std::println(validators_yaml, "    - {}", i);
  }
  validators_yaml.close();
}
