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

#include "types/validator_index.hpp"

namespace lean {
  enum class ValidatorsYamlError {
    INVALID,
  };
  Q_ENUM_ERROR_CODE(ValidatorsYamlError) {
    using E = decltype(e);
    switch (e) {
      case E::INVALID:
        return "Invalid validators.yaml";
    }
    abort();
  }

  using ValidatorsYaml =
      std::unordered_map<std::string, std::vector<ValidatorIndex>>;

  /**
   * Read node ids and associated validator indices from validators.yaml
   */
  inline outcome::result<ValidatorsYaml> readValidatorsYaml(
      const std::filesystem::path &path) {
    ValidatorsYaml result;
    auto yaml = YAML::LoadFile(path);
    if (not yaml.IsMap()) {
      return ValidatorsYamlError::INVALID;
    }
    for (auto &&yaml_kv : yaml) {
      auto &validators = result[yaml_kv.first.as<std::string>()];
      if (not yaml_kv.second.IsSequence()) {
        return ValidatorsYamlError::INVALID;
      }
      for (auto &&yaml_validator : yaml_kv.second) {
        if (not yaml_validator.IsScalar()) {
          return ValidatorsYamlError::INVALID;
        }
        validators.emplace_back(yaml_validator.as<ValidatorIndex>());
      }
    }
    return result;
  }
}  // namespace lean
