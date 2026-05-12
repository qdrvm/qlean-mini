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

#include "app/configuration.hpp"
#include "metrics/metrics.hpp"
#include "serde/yaml.hpp"

namespace lean {
  enum class ValidatorRegistryError {
    MAP_EXPECTED,
    NODE_ID_SCALAR_EXPECTED,
    INVALID_VALUE_TYPE,
    ALREADY_ASSIGNED,
  };
  Q_ENUM_ERROR_CODE(ValidatorRegistryError) {
    using E = decltype(e);
    switch (e) {
      case E::MAP_EXPECTED:
        return "Validator registry YAML must be a YAML map";
      case E::NODE_ID_SCALAR_EXPECTED:
        return "Validator registry YAML has non-scalar node id";
      case E::INVALID_VALUE_TYPE:
        return "Validator registry entry has invalid value type";
      case E::ALREADY_ASSIGNED:
        return "Validator index is already assigned to node";
    }
    abort();
  }

  ValidatorRegistryImpl::ValidatorRegistryImpl(
      qtils::SharedRef<log::LoggingSystem> logging_system,
      qtils::SharedRef<metrics::Metrics> metrics,
      const app::Configuration &config)
      : ValidatorRegistryImpl{
            std::move(logging_system),
            std::move(metrics),
            yaml::read(config.genesisDir() / "annotated_validators.yaml"),
            config.nodeId()} {}

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

  ValidatorRegistry::ValidatorIndices
  ValidatorRegistryImpl::allValidatorsIndices() const {
    ValidatorIndices all_indices;
    all_indices.reserve(index_to_node_.size());
    for (const auto &[idx, _] : index_to_node_) {
      all_indices.insert(idx);
    }
    return all_indices;
  }

  ValidatorRegistryImpl::ValidatorRegistryImpl(
      qtils::SharedRef<log::LoggingSystem> logging_system,
      qtils::SharedRef<metrics::Metrics> metrics,
      yaml::Node yaml,
      std::string current_node_id)
      : logger_{logging_system->getLogger("ValidatorRegistry",
                                          "validator_registry")},
        metrics_{std::move(metrics)},
        current_node_id_{std::move(current_node_id)} {
    for (auto &&[node_id, yaml_items] : yaml.map()) {
      std::vector<ValidatorIndex> indices;
      for (const auto &yaml_item : yaml_items.list()) {
        indices.emplace_back(yaml_item.map("index").num<ValidatorIndex>());
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
        auto it = index_to_node_.find(idx);
        if (it != index_to_node_.end()) {
          if (it->second != node_id) {
            qtils::raise(ValidatorRegistryError::ALREADY_ASSIGNED);
          }
          continue;
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

    metrics_->val_validators_count()->set(current_validator_indices_.size());
  }
}  // namespace lean
