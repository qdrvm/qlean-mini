/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>
#include <fstream>
#include <print>

#include <fmt/format.h>
#include <yaml-cpp/yaml.h>

#include "utils/sample_peer.hpp"

inline int cmdGenerateGenesis(auto &&getArg) {
  auto cmd = [](std::filesystem::path genesis_directory,
                size_t validator_count,
                bool shadow) {
    auto build_yaml = [](std::filesystem::path path, auto &&build) {
      std::ofstream file{path};
      YAML::Node yaml;
      build(yaml);
      file << yaml << "\n";
      file.close();
    };

    auto now = shadow
                 ? std::chrono::seconds{946684800}
                 : std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch());
    uint64_t genesis_time = (now + std::chrono::seconds{3}).count();

    std::filesystem::create_directories(genesis_directory);

    build_yaml(genesis_directory / "config.yaml", [&](YAML::Node &yaml) {
      yaml["GENESIS_TIME"] = genesis_time;
      yaml["VALIDATOR_COUNT"] = validator_count;
    });

    auto node_id = [](size_t index) { return std::format("node_{}", index); };
    std::vector<lean::SamplePeer> peers;
    for (size_t index = 0; index < validator_count; ++index) {
      peers.emplace_back(index, shadow);
    }

    for (auto &peer : peers) {
      std::ofstream node_key{genesis_directory
                             / std::format("{}.key", node_id(peer.index))};
      fmt::println(node_key,
                   "{}",
                   qtils::ByteView{peer.keypair.privateKey.data}.toHex());
      node_key.close();
    }

    build_yaml(genesis_directory / "nodes.yaml", [&](YAML::Node &yaml) {
      for (auto &peer : peers) {
        yaml.push_back(peer.enr);
      }
    });

    build_yaml(genesis_directory / "validators.yaml", [&](YAML::Node &yaml) {
      for (auto &peer : peers) {
        yaml[node_id(peer.index)].push_back(peer.index);
      }
    });

    build_yaml(genesis_directory / "validator-config.yaml",
               [&](YAML::Node &yaml) {
                 yaml["shuffle"] = "roundrobin";
                 for (auto &peer : peers) {
                   YAML::Node yaml_peer;
                   yaml_peer["name"] = node_id(peer.index);
                   yaml_peer["privkey"] =
                       qtils::ByteView{peer.keypair.privateKey.data}.toHex();
                   auto yaml_enr = yaml_peer["enrFields"];
                   yaml_enr["ip"] = lean::enr::toString(peer.enr_ip);
                   yaml_enr["quic"] = peer.port;
                   yaml_peer["count"] = 1;
                   yaml["validators"].push_back(yaml_peer);
                 }
               });
  };
  if (auto arg_2 = getArg(2)) {
    std::filesystem::path genesis_directory{*arg_2};
    if (auto arg_3 = getArg(3)) {
      size_t validator_count = std::stoul(std::string{*arg_3});
      if (validator_count != 0) {
        auto arg_4 = getArg(4);
        auto shadow = arg_4 == "shadow";
        if (not arg_4 or shadow) {
          cmd(genesis_directory, validator_count, shadow);
          return EXIT_SUCCESS;
        }
      }
    }
  }
  auto exe = std::filesystem::path{getArg(0).value()}.filename().string();
  fmt::println(std::cerr,
               "Usage: {} generate-genesis (genesis_directory) "
               "(validator_count) (shadow?)",
               exe);
  return EXIT_FAILURE;
}
