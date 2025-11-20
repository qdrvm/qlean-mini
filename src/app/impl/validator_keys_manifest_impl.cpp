/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app/impl/validator_keys_manifest_impl.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <qtils/unhex.hpp>
#include <yaml-cpp/yaml.h>

#include "app/configuration.hpp"

namespace lean::app {
  ValidatorKeysManifestImpl::ValidatorKeysManifestImpl(
      const Configuration &config) {
    current_node_xmss_keypair_ = config.xmssKeypair();
    const auto &path = config.validatorKeysManifestPath();
    if (path.empty()) {
      throw std::runtime_error("Validator keys manifest path is empty");
    }
    if (not std::filesystem::exists(path)
        || not std::filesystem::is_regular_file(path)) {
      throw std::runtime_error(
          std::string("Validator keys manifest file does not exist: ")
          + path.string());
    }

    YAML::Node root;
    try {
      root = YAML::LoadFile(path.string());
    } catch (const std::exception &e) {
      throw std::runtime_error(
          std::string("Failed to parse validator keys manifest: ") + e.what());
    }

    auto validators_node = root["validators"];
    if (!validators_node || !validators_node.IsSequence()) {
      throw std::runtime_error(
          "Validator keys manifest must contain a 'validators' sequence");
    }

    for (const auto &entry : validators_node) {
      if (!entry.IsMap()) {
        throw std::runtime_error("Each validator entry must be a map");
      }
      auto index_node = entry["index"];
      auto pubkey_node = entry["pubkey_hex"];
      if (!index_node || !pubkey_node) {
        throw std::runtime_error(
            "Validator entry must contain 'index' and 'pubkey_hex'");
      }

      ValidatorIndex index = index_node.as<ValidatorIndex>();
      std::string pubkey_hex = pubkey_node.as<std::string>();
      qtils::ByteVec pubkey_vec;
      auto result = qtils::unhex0x(pubkey_vec, pubkey_hex, true);
      if (not result.has_value()) {
        throw std::runtime_error(
            std::string("Invalid public key hex for validator ")
            + std::to_string(index));
      }

      crypto::xmss::XmssPublicKey pubkey;
      if (pubkey_vec.size() != pubkey.size()) {
        throw std::runtime_error(
            std::string("Invalid public key length for validator ")
            + std::to_string(index));
      }
      std::copy(pubkey_vec.begin(), pubkey_vec.end(), pubkey.begin());

      if (not validator_keys_.emplace(index, std::move(pubkey)).second) {
        throw std::runtime_error(
            std::string("Duplicate validator index in manifest: ")
            + std::to_string(index));
      }
    }
  }

  std::optional<crypto::xmss::XmssPublicKey>
  ValidatorKeysManifestImpl::getXmssPubkeyByIndex(ValidatorIndex index) const {
    auto it = validator_keys_.find(index);
    if (it == validator_keys_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  crypto::xmss::XmssKeypair ValidatorKeysManifestImpl::currentNodeXmssKeypair()
      const {
    return current_node_xmss_keypair_;
  }


  std::vector<crypto::xmss::XmssPublicKey>
  ValidatorKeysManifestImpl::getAllXmssPubkeys() const {
    std::vector<crypto::xmss::XmssPublicKey> pubkeys;
    pubkeys.reserve(validator_keys_.size());
    for (const auto &[_, pubkey] : validator_keys_) {
      pubkeys.push_back(pubkey);
    }
    return pubkeys;
  }


}  // namespace lean::app
