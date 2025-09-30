#include "utils/validator_registry.hpp"

#include <stdexcept>
#include <utility>

#include <fmt/format.h>
#include <yaml-cpp/yaml.h>

namespace lean {

  ValidatorRegistry::ValidatorRegistry(
      qtils::SharedRef<log::LoggingSystem> logging_system,
      const app::Configuration &config)
      : logger_(logging_system->getLogger("ValidatorRegistry",
                                          "validator_registry")),
        registry_path_(config.validatorRegistryPath()),
        current_node_id_(config.nodeId()) {
    loadRegistry();
  }

  const std::filesystem::path &ValidatorRegistry::registryPath() const {
    return registry_path_;
  }

  std::optional<std::string> ValidatorRegistry::nodeIdByIndex(
      ValidatorIndex index) const {
    if (auto it = index_to_node_.find(index); it != index_to_node_.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  std::optional<ValidatorIndex> ValidatorRegistry::validatorIndexForNodeId(
      std::string_view node_id) const {
    if (node_id.empty()) {
      return std::nullopt;
    }
    if (auto it = node_to_indices_.find(std::string(node_id));
        it != node_to_indices_.end() and not it->second.empty()) {
      return it->second.front();
    }
    return std::nullopt;
  }

  ValidatorIndex ValidatorRegistry::currentValidatorIndex() const {
    return current_validator_index_;
  }

  bool ValidatorRegistry::hasCurrentValidatorIndex() const {
    return has_current_validator_index_;
  }

  const std::string &ValidatorRegistry::currentNodeId() const {
    return current_node_id_;
  }

  void ValidatorRegistry::loadRegistry() {
    if (registry_path_.empty()) {
      SL_WARN(
          logger_,
          "Validator registry path is empty; defaulting validator index to 0");
      return;
    }

    YAML::Node root;
    try {
      root = YAML::LoadFile(registry_path_.string());
    } catch (const std::exception &e) {
      throw std::runtime_error(
          fmt::format("Failed to load validator registry '{}': {}",
                      registry_path_.string(),
                      e.what()));
    }

    if (not root.IsDefined() or root.IsNull()) {
      SL_WARN(
          logger_,
          "Validator registry '{}' is empty; defaulting validator index to 0",
          registry_path_.string());
      return;
    }

    if (not root.IsMap()) {
      throw std::runtime_error(
          fmt::format("Validator registry '{}' must be a YAML map",
                      registry_path_.string()));
    }

    for (const auto &entry : root) {
      if (not entry.first.IsScalar()) {
        throw std::runtime_error(
            fmt::format("Validator registry '{}' has non-scalar node id",
                        registry_path_.string()));
      }
      auto node_id = entry.first.as<std::string>();
      const auto &indices_node = entry.second;

      std::vector<ValidatorIndex> indices;
      auto parse_index = [&](const YAML::Node &value) {
        if (not value.IsScalar()) {
          throw std::runtime_error(fmt::format(
              "Validator registry entry '{}' must contain scalar indices",
              node_id));
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
        throw std::runtime_error(fmt::format(
            "Validator registry entry '{}' has invalid value type", node_id));
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
          throw std::runtime_error(
              fmt::format("Validator index {} is already assigned to node '{}' "
                          "(while processing '{}')",
                          idx,
                          index_to_node_.at(idx),
                          node_id));
        }
        index_to_node_.emplace(idx, node_id);
        node_indices.emplace_back(idx);
      }
    }

    if (current_node_id_.empty()) {
      SL_WARN(logger_, "Node id is not set; defaulting validator index to 0");
      return;
    }

    if (auto opt_index = validatorIndexForNodeId(current_node_id_);
        opt_index.has_value()) {
      current_validator_index_ = opt_index.value();
      has_current_validator_index_ = true;
      SL_INFO(logger_,
              "Node '{}' mapped to validator index {}",
              current_node_id_,
              current_validator_index_);
    } else {
      SL_WARN(logger_,
              "Validator index for node '{}' not found in registry '{}'; "
              "defaulting to 0",
              current_node_id_,
              registry_path_.string());
      current_validator_index_ = 0;
      has_current_validator_index_ = false;
    }
  }

}  // namespace lean
