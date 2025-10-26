/**
* Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>
#include <vector>

#include <qtils/read_file.hpp>
#include <qtils/unhex.hpp>
#include <yaml-cpp/yaml.h>

#include "serde/enr.hpp"

namespace lean {
  struct ParsedEnr {
    std::string raw;
    enr::Enr enr;
  };

  struct NodesYamlParseResult {
    std::vector<ParsedEnr> parsed;
    std::vector<std::string> invalid_entries;
  };

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
  inline outcome::result<NodesYamlParseResult> readNodesYaml(
      const std::filesystem::path &path) {
    NodesYamlParseResult result;
    auto yaml = YAML::LoadFile(path);
    if (not yaml.IsSequence()) {
      return NodesYamlError::INVALID;
    }
    for (auto &&yaml_enr : yaml) {
      if (not yaml_enr.IsScalar()) {
        return NodesYamlError::INVALID;
      }
      auto enr_str = yaml_enr.as<std::string>();
      if (enr_str.empty()) {
        result.invalid_entries.emplace_back(std::move(enr_str));
        continue;
      }
      auto enr_res = enr::decode(enr_str);
      if (not enr_res.has_value()) {
        result.invalid_entries.emplace_back(std::move(enr_str));
        continue;
      }
      result.parsed.emplace_back(ParsedEnr{std::move(enr_str),
                                           std::move(enr_res.value())});
    }
    return result;
  }
}  // namespace lean