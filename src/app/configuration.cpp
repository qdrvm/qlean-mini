/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app/configuration.hpp"

namespace lean::app {

  Configuration::Configuration()
      : version_("undefined"),
        name_("unnamed"),
        database_{
            .directory = "db",
            .cache_size = 1 << 30,
        },
        metrics_{
            .endpoint{},
            .enabled{},
        } {}

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

  const std::filesystem::path &Configuration::validatorRegistryPath() const {
    return validator_registry_path_;
  }

  const std::filesystem::path &Configuration::genesisConfigPath() const {
    return genesis_config_path_;
  }

  const std::optional<std::string> &Configuration::listenMultiaddr() const {
    return listen_multiaddr_;
  }

  const std::optional<std::string> &Configuration::nodeKeyHex() const {
    return node_key_hex_;
  }

  const Configuration::DatabaseConfig &Configuration::database() const {
    return database_;
  }

  const Configuration::MetricsConfig &Configuration::metrics() const {
    return metrics_;
  }

}  // namespace lean::app
