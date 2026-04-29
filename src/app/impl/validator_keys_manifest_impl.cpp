/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app/impl/validator_keys_manifest_impl.hpp"

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <stdexcept>
#include <string>

#include <qtils/unhex.hpp>

#include "app/configuration.hpp"
#include "crypto/xmss/xmss_provider_fake.hpp"
#include "crypto/xmss/xmss_util.hpp"
#include "executable/qlean_enable_shadow.hpp"
#include "serde/yaml.hpp"

namespace lean::app {
  ValidatorKeysManifestImpl::ValidatorKeysManifestImpl(
      const Configuration &config) {
    auto hashsig_dir = config.genesisDir() / "hash-sig-keys";
    auto yaml = yaml::read(hashsig_dir / "validator-keys-manifest.yaml");
    for (auto &&yaml_item : yaml.map("validators").list()) {
      auto index = yaml_item.map("index").num<ValidatorIndex>();
      auto yaml_pubkey_hex = yaml_item.map("pubkey_hex");
      crypto::xmss::XmssPublicKey public_key;
      qtils::unhex0x(public_key, yaml_pubkey_hex.str(), true).value();
      auto privkey_file = yaml_item.map("privkey_file").str();
      crypto::xmss::XmssKeypair keypair;
      if constexpr (QLEAN_ENABLE_SHADOW) {
        keypair = crypto::xmss::XmssProviderFake::loadKeypair(public_key,
                                                              privkey_file);
      } else {
        keypair = crypto::xmss::loadKeypair(
                      public_key,
                      config.genesisDir() / "hash-sig-keys" / privkey_file)
                      .value();
      }
      validator_keys_.emplace(public_key, keypair);
    }
  }

  std::optional<crypto::xmss::XmssKeypair>
  ValidatorKeysManifestImpl::getKeypair(
      const crypto::xmss::XmssPublicKey &public_key) const {
    auto it = validator_keys_.find(public_key);
    if (it == validator_keys_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  std::vector<crypto::xmss::XmssPublicKey>
  ValidatorKeysManifestImpl::getAllXmssPubkeys() const {
    std::vector<crypto::xmss::XmssPublicKey> pubkeys;
    pubkeys.reserve(validator_keys_.size());
    for (const auto &pubkey : validator_keys_ | std::views::keys) {
      pubkeys.push_back(pubkey);
    }
    return pubkeys;
  }
}  // namespace lean::app
