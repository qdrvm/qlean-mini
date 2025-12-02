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

#include "blockchain/state_transition_function.hpp"

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
  inline outcome::result<State> readConfigYaml(
      const std::filesystem::path &path) {
    auto yaml = YAML::LoadFile(path);
    if (not yaml.IsMap()) {
      return ConfigYamlError::INVALID;
    }
    auto yaml_genesis_time = yaml["GENESIS_TIME"];
    if (not yaml_genesis_time.IsScalar()) {
      return ConfigYamlError::INVALID;
    }
    auto yaml_genesis_validators = yaml["GENESIS_VALIDATORS"];
    if (not yaml_genesis_validators.IsSequence()) {
      return ConfigYamlError::INVALID;
    }
    std::vector<crypto::xmss::XmssPublicKey> validators;
    for (auto &&yaml_validator : yaml_genesis_validators) {
      if (not yaml_validator.IsScalar()) {
        return ConfigYamlError::INVALID;
      }
      auto validator_str = yaml_validator.as<std::string>();
      BOOST_OUTCOME_TRY(auto validator,
                        crypto::xmss::XmssPublicKey::fromHex(validator_str));
      validators.emplace_back(validator);
    }
    Config config{.genesis_time = yaml_genesis_time.as<uint64_t>()};
    return STF::generateGenesisState(config, validators);
  }
}  // namespace lean
