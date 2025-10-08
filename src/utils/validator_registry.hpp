#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <log/logger.hpp>

#include "app/configuration.hpp"
#include "types/validator_index.hpp"

namespace lean {

  /**
   * ValidatorRegistry provides access to mapping between validator indices and
   * node identifiers. Mapping is loaded from YAML file with structure:
   *
   * ```yaml
   * node_0:
   *   - 0
   * node_1:
   *   - 1
   * ```
   */
  class ValidatorRegistry {
   public:
    using ValidatorIndices = std::unordered_set<ValidatorIndex>;

    ValidatorRegistry(qtils::SharedRef<log::LoggingSystem> logging_system,
                      const app::Configuration &config);

    static ValidatorRegistry createForTesting(
        qtils::SharedRef<log::LoggingSystem> logging_system,
        std::filesystem::path registry_path,
        std::string current_node_id);

    [[nodiscard]] const std::filesystem::path &registryPath() const;

    [[nodiscard]] std::optional<std::string> nodeIdByIndex(
        ValidatorIndex index) const;

    [[nodiscard]] std::optional<ValidatorIndices> validatorIndicesForNodeId(
        std::string_view node_id) const;

    [[nodiscard]] const ValidatorIndices &currentValidatorIndices() const;

    [[nodiscard]] const std::string &currentNodeId() const;

   private:
    ValidatorRegistry(qtils::SharedRef<log::LoggingSystem> logging_system,
                      std::filesystem::path registry_path,
                      std::string current_node_id);

    void loadRegistry();

    log::Logger logger_;
    std::filesystem::path registry_path_;
    std::string current_node_id_;
    std::unordered_map<ValidatorIndex, std::string> index_to_node_;
    std::unordered_map<std::string, std::vector<ValidatorIndex>>
        node_to_indices_;
    ValidatorIndices current_validator_indices_;
  };

}  // namespace lean
