/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app/configuration.hpp"

#include "executable/qlean_enable_shadow.hpp"

namespace lean::app {

  Configuration::Configuration()
      : version_("undefined"),
        name_("unnamed"),
        database_{
            .directory = "db",
            .cache_size = 1 << 30,
        },
        metrics_{
            .endpoint{boost::asio::ip::make_address("127.0.0.1"), 9668},
            .enabled{},
        },
        api_endpoint_{boost::asio::ip::make_address("127.0.0.1"), 9667} {}

  const std::string &Configuration::nodeVersion() const {
    return version_;
  }

  const std::string &Configuration::nodeName() const {
    return name_;
  }

  const std::string &Configuration::nodeId() const {
    return node_id_;
  }

  const std::filesystem::path &Configuration::basePath() const {
    return base_path_;
  }

  const std::filesystem::path &Configuration::modulesDir() const {
    return modules_dir_;
  }

  const std::filesystem::path &Configuration::bootnodesFile() const {
    return bootnodes_file_;
  }

  const std::optional<std::string> &Configuration::stateSyncUrl() const {
    return state_sync_url_;
  }

  const std::filesystem::path &Configuration::validatorRegistryPath() const {
    return validator_registry_path_;
  }

  const std::filesystem::path &Configuration::genesisConfigPath() const {
    return genesis_config_path_;
  }

  const std::optional<libp2p::Multiaddress> &Configuration::listenMultiaddr()
      const {
    return listen_multiaddr_;
  }

  const libp2p::crypto::KeyPair &Configuration::nodeKey() const {
    return node_key_.value();
  }

  const crypto::xmss::XmssKeypair &Configuration::xmssKeypair() const {
    return xmss_keypair_.value();
  }


  const std::optional<size_t> &Configuration::maxBootnodes() const {
    return max_bootnodes_;
  }

  const std::filesystem::path &Configuration::validatorKeysManifestPath()
      const {
    return validator_keys_manifest_path_;
  }

  double Configuration::fakeXmssAggregateSignaturesRate() const {
    ASSERT_QLEAN_ENABLE_SHADOW();
    return fake_xmss_aggregate_signatures_rate_;
  }

  double Configuration::fakeXmssVerifyAggregatedSignaturesRate() const {
    ASSERT_QLEAN_ENABLE_SHADOW();
    return fake_xmss_verify_aggregated_signatures_rate_;
  }

  const Configuration::DatabaseConfig &Configuration::database() const {
    return database_;
  }

  const Configuration::MetricsConfig &Configuration::metrics() const {
    return metrics_;
  }

  const Configuration::Endpoint &Configuration::apiEndpoint() const {
    return api_endpoint_;
  }
}  // namespace lean::app
