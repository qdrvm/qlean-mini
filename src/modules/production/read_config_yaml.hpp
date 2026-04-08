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
    auto yaml_genesis_validators = yaml["VALIDATORS"];
    if (not yaml_genesis_validators.IsSequence()) {
      yaml_genesis_validators = yaml["GENESIS_VALIDATORS"];
      if (not yaml_genesis_validators.IsSequence()) {
        return ConfigYamlError::INVALID;
      }
    }
    std::vector<Validator> validators;
    for (auto &&[i, yaml_validator] :
         std::views::zip(std::views::iota(yaml_genesis_validators.size()),
                         yaml_genesis_validators)) {
      if (not yaml_validator.IsMap()) {
        return ConfigYamlError::INVALID;
      }
      auto yaml_attestation_pubkey = yaml_validator["attestation_pubkey"];
      if (not yaml_attestation_pubkey.IsScalar()) {
        return ConfigYamlError::INVALID;
      }
      BOOST_OUTCOME_TRY(auto attestation_pubkey,
                        crypto::xmss::XmssPublicKey::fromHex(
                            yaml_attestation_pubkey.as<std::string>()));
      auto yaml_proposal_pubkey = yaml_validator["proposal_pubkey"];
      if (not yaml_proposal_pubkey.IsScalar()) {
        return ConfigYamlError::INVALID;
      }
      BOOST_OUTCOME_TRY(auto proposal_pubkey,
                        crypto::xmss::XmssPublicKey::fromHex(
                            yaml_proposal_pubkey.as<std::string>()));
      validators.emplace_back(Validator{
          .attestation_pubkey = attestation_pubkey,
          .proposal_pubkey = proposal_pubkey,
          .index = i,
      });
    }
    Config config{
        .genesis_time = yaml_genesis_time.as<uint64_t>(),
    };
    return STF::generateGenesisState(config, validators);
  }
}  // namespace lean
