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
#include "types/validator.hpp"
#include "utils/getenv.hpp"
#include "utils/sample_peer.hpp"

inline outcome::result<lean::crypto::xmss::XmssPublicKey> generateXmss(
    size_t index,
    const std::filesystem::path &pk_path,
    const std::filesystem::path &sk_path,
    uint32_t xmss_activation_epoch,
    uint32_t xmss_active_epoch) {
  auto write = [](const std::filesystem::path &path, qtils::BytesIn bytes) {
    std::ofstream{path}
        .write(qtils::byte2str(bytes.data()), bytes.size())
        .flush();
  };
  auto symlink = [](const std::filesystem::path &target,
                    const std::filesystem::path &link) {
    std::filesystem::remove(link);
    std::filesystem::create_symlink(target, link);
  };

  auto pk_cache_path = pk_path;
  auto sk_cache_path = sk_path;
  std::optional<std::filesystem::path> cache_dir;
  if (auto s = lean::getEnv("QLEAN_XMSS_CACHE")) {
    cache_dir = std::filesystem::absolute(*s);
  }
  if (cache_dir.has_value()) {
    std::filesystem::create_directories(*cache_dir);
    pk_cache_path = *cache_dir / std::format("{}_pk.ssz", index);
    sk_cache_path = *cache_dir / std::format("{}_sk.ssz", index);
  }
  lean::crypto::xmss::XmssKeypair keypair;
  if (std::filesystem::exists(pk_cache_path)
      and std::filesystem::exists(sk_cache_path)) {
    auto keypair_result =
        lean::crypto::xmss::loadKeypair(sk_cache_path, pk_cache_path);
    if (not keypair_result) {
      fmt::println(std::cerr,
                   "Error loading XMSS keypair: {}",
                   keypair_result.error().message());
      fmt::println(std::cerr, "  {}", pk_path.string());
      fmt::println(std::cerr, "  {}", sk_path.string());
      return keypair_result.error();
    }
    keypair = keypair_result.value();
  } else {
    fmt::println(std::cerr, "Generating XMSS keypair {}", index);
    auto keypair = lean::crypto::xmss::XmssProviderImpl{}.generateKeypair(
        xmss_activation_epoch, xmss_active_epoch);
    write(sk_cache_path, lean::crypto::xmss::toBytes(keypair.private_key));
    write(pk_cache_path, keypair.public_key);
  }
  if (cache_dir.has_value()) {
    symlink(pk_cache_path, pk_path);
    symlink(sk_cache_path, sk_path);
  }
  return keypair.public_key;
}

inline int cmdGenerateGenesis(auto &&getArg) {
  auto cmd = [](std::filesystem::path genesis_directory,
                size_t validator_count,
                size_t subnet_count,
                bool shadow,
                bool fake_xmss) {
    auto build_yaml = [](std::filesystem::path path, auto &&build) {
      std::ofstream file{path};
      YAML::Node yaml;
      build(yaml);
      file << yaml << "\n";
      file.close();
    };
    auto pk_attester_name = [](lean::ValidatorIndex index) {
      return std::format("validator_{}_attester_key_pk.ssz", index);
    };
    auto sk_attester_name = [](lean::ValidatorIndex index) {
      return std::format("validator_{}_attester_key_sk.ssz", index);
    };
    auto pk_proposer_name = [](lean::ValidatorIndex index) {
      return std::format("validator_{}_proposer_key_pk.ssz", index);
    };
    auto sk_proposer_name = [](lean::ValidatorIndex index) {
      return std::format("validator_{}_proposer_key_sk.ssz", index);
    };

    if (subnet_count > validator_count) {
      fmt::println(std::cerr, "subnet_count must not exceed validator_count");
      return EXIT_FAILURE;
    }

    auto now = shadow
                 ? std::chrono::seconds{946684800}
                 : std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch());
    uint64_t genesis_time = (now + std::chrono::seconds{3}).count();

    const auto xmss_activation_epoch = 0;
    const auto xmss_active_epoch_log = 18;
    const auto xmss_active_epoch = uint64_t{1} << xmss_active_epoch_log;

    std::filesystem::create_directories(genesis_directory);

    auto hashsig_directory = genesis_directory / "hash-sig-keys";
    std::filesystem::create_directories(hashsig_directory);

    std::vector<lean::Validator> xmss_public_keys;
    if (not fake_xmss) {
      for (size_t index = 0; index < validator_count; ++index) {
        auto pk_attester_result =
            generateXmss(2 * index,
                         hashsig_directory / pk_attester_name(index),
                         hashsig_directory / sk_attester_name(index),
                         xmss_activation_epoch,
                         xmss_active_epoch);
        auto pk_proposer_result =
            generateXmss(2 * index + 1,
                         hashsig_directory / pk_proposer_name(index),
                         hashsig_directory / sk_proposer_name(index),
                         xmss_activation_epoch,
                         xmss_active_epoch);
        if (not pk_attester_result.has_value()
            or not pk_proposer_result.has_value()) {
          return EXIT_FAILURE;
        }
        xmss_public_keys.emplace_back(lean::Validator{
            .attestation_pubkey = pk_attester_result.value(),
            .proposal_pubkey = pk_proposer_result.value(),
        });
      }
    } else {
      xmss_public_keys.resize(validator_count);
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
            yaml_validator["attester_key_pubkey_hex"] =
                "0x" + xmss_public_keys.at(index).attestation_pubkey.toHex();
            yaml_validator["attester_key_privkey_file"] =
                sk_attester_name(index);
            yaml_validator["proposer_key_pubkey_hex"] =
                "0x" + xmss_public_keys.at(index).proposal_pubkey.toHex();
            yaml_validator["proposer_key_privkey_file"] =
                sk_proposer_name(index);
            yaml_validators.push_back(yaml_validator);
          }
        });

    build_yaml(genesis_directory / "config.yaml", [&](YAML::Node &yaml) {
      yaml["GENESIS_TIME"] = genesis_time;
      yaml["VALIDATOR_COUNT"] = validator_count;
      auto &&yaml_validators = yaml["GENESIS_VALIDATORS"];
      for (auto &xmss_public_key : xmss_public_keys) {
        YAML::Node yaml_validator;
        yaml_validator["attestation_pubkey"] =
            xmss_public_key.attestation_pubkey.toHex();
        yaml_validator["proposal_pubkey"] =
            xmss_public_key.proposal_pubkey.toHex();
        yaml_validators.push_back(yaml_validator);
      }
    });

    auto node_id = [](size_t index) { return std::format("node_{}", index); };
    std::vector<lean::SamplePeer> peers;
    for (size_t index = 0; index < validator_count; ++index) {
      // first peer in subnet is aggregator
      auto is_aggregator = index < subnet_count;
      peers.emplace_back(index, is_aggregator, shadow);
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

    build_yaml(genesis_directory / "annotated_validators.yaml",
               [&](YAML::Node &yaml) {
                 for (auto &peer : peers) {
                   auto add = [&](auto &pk, auto &sk_name) {
                     YAML::Node entry;
                     entry["index"] = peer.index;
                     entry["pubkey_hex"] = "0x" + pk.toHex();
                     entry["privkey_file"] = sk_name(peer.index);
                     yaml[node_id(peer.index)].push_back(entry);
                   };
                   auto &pk = xmss_public_keys.at(peer.index);
                   add(pk.attestation_pubkey, sk_attester_name);
                   add(pk.proposal_pubkey, sk_proposer_name);
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
                   yaml_enr["is_aggregator"] = peer.is_aggregator;
                   yaml_peer["count"] = 1;
                   yaml_peer["metricsPort"] = 8080 + peer.index;
                   yaml["validators"].push_back(yaml_peer);
                 }
               });
    return EXIT_SUCCESS;
  };
  auto help =
      [exe{std::filesystem::path{getArg(0).value()}.filename().string()}] {
        fmt::println(
            std::cerr,
            "Usage: {} generate-genesis (genesis_directory) "
            "(validator_count) (subnet_count) (shadow-ip?) (fake-xmss?)",
            exe);
        return EXIT_FAILURE;
      };
  auto arg_2 = getArg(2);
  if (not arg_2.has_value()) {
    return help();
  }
  std::filesystem::path genesis_directory{*arg_2};
  auto arg_3 = getArg(3);
  if (not arg_3.has_value()) {
    return help();
  }
  size_t validator_count = std::stoul(std::string{*arg_3});
  if (validator_count == 0) {
    return help();
  }
  auto arg_4 = getArg(4);
  if (not arg_4.has_value()) {
    return help();
  }
  size_t subnet_count = std::stoul(std::string{*arg_4});
  if (subnet_count == 0) {
    return help();
  }
  auto shadow = false;
  auto fake_xmss = false;
  for (auto i = 5;; ++i) {
    auto arg = getArg(i);
    if (not arg.has_value()) {
      break;
    }
    if (arg == "shadow") {
      shadow = true;
      continue;
    }
    if (arg == "fake-xmss") {
      fake_xmss = true;
      continue;
    }
    return help();
  }
  return cmd(
      genesis_directory, validator_count, subnet_count, shadow, fake_xmss);
}
