#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
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
    ValidatorRegistry(qtils::SharedRef<log::LoggingSystem> logging_system,
                      const app::Configuration &config);

    static ValidatorRegistry createForTesting(
        qtils::SharedRef<log::LoggingSystem> logging_system,
        std::filesystem::path registry_path,
        std::string current_node_id);

    [[nodiscard]] const std::filesystem::path &registryPath() const;

    [[nodiscard]] std::optional<std::string> nodeIdByIndex(
        ValidatorIndex index) const;

    [[nodiscard]] std::optional<ValidatorIndex> validatorIndexForNodeId(
        std::string_view node_id) const;

    [[nodiscard]] ValidatorIndex currentValidatorIndex() const;

    [[nodiscard]] bool hasCurrentValidatorIndex() const;

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
    ValidatorIndex current_validator_index_ = 0;
    bool has_current_validator_index_ = false;
  };

}  // namespace lean
