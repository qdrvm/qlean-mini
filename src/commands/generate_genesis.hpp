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

#include "crypto/xmss/xmss_provider_impl.hpp"
#include "crypto/xmss/xmss_util.cpp"
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
    auto xmss_public_key_name = [](lean::ValidatorIndex index) {
      return std::format("validator_{}_pk.json", index);
    };
    auto xmss_private_key_name = [](lean::ValidatorIndex index) {
      return std::format("validator_{}_sk.json", index);
    };
    auto write_json = [](const std::filesystem::path &path, const auto &key) {
      auto json = lean::crypto::xmss::toJson(key);
      std::ofstream{path}.write(json.data(), json.size()).flush();
    };

    auto now = shadow
                 ? std::chrono::seconds{946684800}
                 : std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch());
    uint64_t genesis_time = (now + std::chrono::seconds{3}).count();

    const auto xmss_activation_epoch = 0;
    const auto xmss_active_epoch_log = 18;
    const auto xmss_active_epoch =
        std::pow(std::uint64_t{2}, xmss_active_epoch_log);

    std::filesystem::create_directories(genesis_directory);

    auto hashsig_directory = genesis_directory / "hash-sig-keys";
    std::filesystem::create_directories(hashsig_directory);

    std::vector<lean::crypto::xmss::XmssPublicKey> xmss_public_keys;
    for (size_t index = 0; index < validator_count; ++index) {
      auto xmss_public_key_path =
          hashsig_directory / xmss_public_key_name(index);
      auto xmss_private_key_path =
          hashsig_directory / xmss_private_key_name(index);
      if (std::filesystem::exists(xmss_public_key_path)
          and std::filesystem::exists(xmss_private_key_path)) {
        auto keypair_result = lean::crypto::xmss::loadKeypairFromJson(
            xmss_private_key_path, xmss_public_key_path);
        if (not keypair_result) {
          fmt::println(std::cerr,
                       "Error loading XMSS keypair: {}",
                       keypair_result.error().message());
          fmt::println(std::cerr, "  {}", xmss_public_key_path.string());
          fmt::println(std::cerr, "  {}", xmss_private_key_path.string());
          return EXIT_FAILURE;
        }
        auto &keypair = keypair_result.value();
        xmss_public_keys.emplace_back(keypair.public_key);
      } else {
        fmt::println(
            std::cerr, "Generating XMSS keypair for validator {}", index);
        auto keypair = lean::crypto::xmss::XmssProviderImpl{}.generateKeypair(
            xmss_activation_epoch, xmss_active_epoch);
        write_json(xmss_private_key_path, keypair.private_key);
        write_json(xmss_public_key_path, keypair.public_key);
        xmss_public_keys.emplace_back(keypair.public_key);
      }
    }

    build_yaml(
        hashsig_directory / "validator-keys-manifest.yaml",
        [&](YAML::Node &yaml) {
          yaml["key_scheme"] = "SIGTopLevelTargetSumLifetime32Dim64Base8";
          yaml["hash_function"] = "Poseidon2";
          yaml["encoding"] = "TargetSum";
          yaml["lifetime"] = 4294967296;
          yaml["log_num_active_epochs"] = xmss_active_epoch_log;
          yaml["num_active_epochs"] = xmss_active_epoch;
          yaml["num_validators"] = validator_count;
          auto &&yaml_validators = yaml["validators"];
          for (size_t index = 0; index < validator_count; ++index) {
            YAML::Node yaml_validator;
            yaml_validator["index"] = index;
            yaml_validator["pubkey_hex"] =
                "0x" + xmss_public_keys.at(index).toHex();
            yaml_validator["privkey_file"] = xmss_private_key_name(index);
            yaml_validators.push_back(yaml_validator);
          }
        });

    build_yaml(genesis_directory / "config.yaml", [&](YAML::Node &yaml) {
      yaml["GENESIS_TIME"] = genesis_time;
      yaml["VALIDATOR_COUNT"] = validator_count;
      auto &&yaml_validators = yaml["VALIDATORS"];
      for (auto &xmss_public_key : xmss_public_keys) {
        yaml_validators.push_back(xmss_public_key.toHex());
      }
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
                 auto &&yaml_config = yaml["config"];
                 yaml_config["activeEpoch"] = xmss_active_epoch_log;
                 yaml_config["keyType"] = "hash-sig";
                 for (auto &peer : peers) {
                   YAML::Node yaml_peer;
                   yaml_peer["name"] = node_id(peer.index);
                   yaml_peer["privkey"] =
                       qtils::ByteView{peer.keypair.privateKey.data}.toHex();
                   auto yaml_enr = yaml_peer["enrFields"];
                   yaml_enr["ip"] = lean::enr::toString(peer.enr_ip);
                   yaml_enr["quic"] = peer.port;
                   yaml_peer["count"] = 1;
                   yaml_peer["metricsPort"] = 8080 + peer.index;
                   yaml["validators"].push_back(yaml_peer);
                 }
               });
    return EXIT_SUCCESS;
  };
  if (auto arg_2 = getArg(2)) {
    std::filesystem::path genesis_directory{*arg_2};
    if (auto arg_3 = getArg(3)) {
      size_t validator_count = std::stoul(std::string{*arg_3});
      if (validator_count != 0) {
        auto arg_4 = getArg(4);
        auto shadow = arg_4 == "shadow";
        if (not arg_4 or shadow) {
          return cmd(genesis_directory, validator_count, shadow);
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
