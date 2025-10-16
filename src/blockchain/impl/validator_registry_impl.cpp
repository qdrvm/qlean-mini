/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/impl/validator_registry_impl.hpp"

#include <stdexcept>
#include <utility>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <yaml-cpp/yaml.h>

#include "app/configuration.hpp"

namespace lean {
  ValidatorRegistryImpl::ValidatorRegistryImpl(
      qtils::SharedRef<log::LoggingSystem> logging_system,
      const app::Configuration &config)
      : ValidatorRegistryImpl{std::move(logging_system),
                              YAML::LoadFile(config.validatorRegistryPath()),
                              config.nodeId()} {}

  ValidatorRegistryImpl::ValidatorRegistryImpl(
      qtils::SharedRef<log::LoggingSystem> logging_system,
      std::string yaml,
      std::string current_node_id)
      : ValidatorRegistryImpl{std::move(logging_system),
                              YAML::Load(yaml),
                              std::move(current_node_id)} {}

  std::optional<std::string> ValidatorRegistryImpl::nodeIdByIndex(
      ValidatorIndex index) const {
    if (auto it = index_to_node_.find(index); it != index_to_node_.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  std::optional<ValidatorRegistry::ValidatorIndices>
  ValidatorRegistryImpl::validatorIndicesForNodeId(
      std::string_view node_id) const {
    if (node_id.empty()) {
      return std::nullopt;
    }
    if (auto it = node_to_indices_.find(std::string(node_id));
        it != node_to_indices_.end() and not it->second.empty()) {
      return ValidatorIndices{it->second.begin(), it->second.end()};
    }
    return std::nullopt;
  }

  const ValidatorRegistry::ValidatorIndices &
  ValidatorRegistryImpl::currentValidatorIndices() const {
    return current_validator_indices_;
  }

  ValidatorRegistryImpl::ValidatorRegistryImpl(
      qtils::SharedRef<log::LoggingSystem> logging_system,
      YAML::Node root,
      std::string current_node_id)
      : logger_{logging_system->getLogger("ValidatorRegistry",
                                          "validator_registry")},
        current_node_id_{std::move(current_node_id)} {
    if (not root.IsDefined() or root.IsNull()) {
      SL_WARN(logger_, "Validator registry YAML is empty");
      return;
    }

    if (not root.IsMap()) {
      throw std::runtime_error{"Validator registry YAML must be a YAML map"};
    }

    for (const auto &entry : root) {
      if (not entry.first.IsScalar()) {
        throw std::runtime_error{
            "Validator registry YAML has non-scalar node id"};
      }
      auto node_id = entry.first.as<std::string>();
      const auto &indices_node = entry.second;

      std::vector<ValidatorIndex> indices;
      auto parse_index = [&](const YAML::Node &value) {
        if (not value.IsScalar()) {
          throw std::runtime_error{fmt::format(
              "Validator registry entry '{}' must contain scalar indices",
              node_id)};
        }
        auto idx = value.as<ValidatorIndex>();
        indices.emplace_back(idx);
      };

      if (indices_node.IsSequence()) {
        for (const auto &child : indices_node) {
          parse_index(child);
        }
      } else if (indices_node.IsScalar()) {
        parse_index(indices_node);
      } else if (indices_node.IsNull()) {
        continue;
      } else {
        throw std::runtime_error{fmt::format(
            "Validator registry entry '{}' has invalid value type", node_id)};
      }

      if (indices.empty()) {
        SL_WARN(logger_,
                "Validator registry entry '{}' has no validator indices",
                node_id);
        continue;
      }

      auto &node_indices = node_to_indices_[node_id];
      node_indices.reserve(node_indices.size() + indices.size());

      for (auto idx : indices) {
        if (index_to_node_.contains(idx)) {
          throw std::runtime_error{
              fmt::format("Validator index {} is already assigned to node '{}' "
                          "(while processing '{}')",
                          idx,
                          index_to_node_.at(idx),
                          node_id)};
        }
        index_to_node_.emplace(idx, node_id);
        node_indices.emplace_back(idx);
      }
    }

    if (current_node_id_.empty()) {
      SL_WARN(logger_, "Node id is not set; defaulting validator index to 0");
      return;
    }

    if (auto opt_indices = validatorIndicesForNodeId(current_node_id_);
        opt_indices.has_value()) {
      current_validator_indices_ = opt_indices.value();
      SL_INFO(logger_,
              "Node '{}' mapped to validator indices {}",
              current_node_id_,
              fmt::join(current_validator_indices_, " "));
    } else {
      SL_WARN(logger_,
              "Validator indices for node '{}' not found in registry YAML",
              current_node_id_);
    }
  }
}  // namespace lean
