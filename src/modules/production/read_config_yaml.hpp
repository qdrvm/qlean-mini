/**
* Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>

#include <qtils/enum_error_code.hpp>
#include <qtils/outcome.hpp>
#include <yaml-cpp/yaml.h>

#include "types/config.hpp"

namespace lean {
  enum class ConfigYamlError {
    INVALID,
  };
  Q_ENUM_ERROR_CODE(ConfigYamlError) {
    using E = decltype(e);
    switch (e) {
      case E::INVALID:
        return "Invalid config.yaml";
    }
    abort();
  }

  /**
   * Read GENESIS_TIME and VALIDATOR_COUNT from config.yaml
   */
  inline outcome::result<Config> readConfigYaml(
      const std::filesystem::path &path) {
    auto yaml = YAML::LoadFile(path);
    if (not yaml.IsMap()) {
      return ConfigYamlError::INVALID;
    }
    auto yaml_genesis_time = yaml["GENESIS_TIME"];
    if (not yaml_genesis_time.IsScalar()) {
      return ConfigYamlError::INVALID;
    }
    auto yaml_validator_count = yaml["VALIDATOR_COUNT"];
    if (not yaml_validator_count.IsScalar()) {
      return ConfigYamlError::INVALID;
    }
    return Config{
      .num_validators = yaml_validator_count.as<uint64_t>(),
      .genesis_time = yaml_genesis_time.as<uint64_t>(),
  };
  }
}  // namespace lean