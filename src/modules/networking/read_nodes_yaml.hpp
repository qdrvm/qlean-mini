/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/read_file.hpp>
#include <qtils/unhex.hpp>
#include <yaml-cpp/yaml.h>

#include "serde/enr.hpp"

namespace lean {
  enum class NodesYamlError {
    INVALID,
  };
  Q_ENUM_ERROR_CODE(NodesYamlError) {
    using E = decltype(e);
    switch (e) {
      case E::INVALID:
        return "Invalid nodes.yaml";
    }
    abort();
  }

  /**
   * Read ENR from nodes.yaml
   */
  inline outcome::result<std::vector<enr::Enr>> readNodesYaml(
      const std::filesystem::path &path) {
    std::vector<enr::Enr> enrs;
    auto yaml = YAML::LoadFile(path);
    if (not yaml.IsSequence()) {
      return NodesYamlError::INVALID;
    }
    for (auto &&yaml_enr : yaml) {
      if (not yaml_enr.IsScalar()) {
        return NodesYamlError::INVALID;
      }
      auto enr_str = yaml_enr.as<std::string>();
      BOOST_OUTCOME_TRY(auto enr, enr::decode(enr_str));
      enrs.emplace_back(std::move(enr));
    }
    return enrs;
  }
}  // namespace lean
