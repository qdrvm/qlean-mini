/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <unordered_map>

#include <boost/di.hpp>
#include <log/logger.hpp>
#include <qtils/shared_ref.hpp>

#include "blockchain/validator_registry.hpp"

namespace lean::app {
  class Configuration;
}  // namespace lean::app

namespace lean::metrics {
  class Metrics;
}  // namespace lean::metrics

namespace lean::yaml {
  struct Node;
}  // namespace lean::yaml

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
  class ValidatorRegistryImpl : public ValidatorRegistry {
   public:
    ValidatorRegistryImpl(qtils::SharedRef<log::LoggingSystem> logging_system,
                          qtils::SharedRef<metrics::Metrics> metrics,
                          const app::Configuration &config);
    BOOST_DI_INJECT_TRAITS(qtils::SharedRef<lean::log::LoggingSystem>,
                           qtils::SharedRef<lean::metrics::Metrics>,
                           const lean::app::Configuration &);

    // ValidatorRegistry
    [[nodiscard]] const ValidatorIndices &currentValidatorIndices()
        const override;

    [[nodiscard]] ValidatorIndices allValidatorsIndices() const override;

    [[nodiscard]] std::optional<std::string> nodeIdByIndex(
        ValidatorIndex index) const override;

    [[nodiscard]] std::optional<ValidatorIndices> validatorIndicesForNodeId(
        std::string_view node_id) const override;

   private:
    ValidatorRegistryImpl(qtils::SharedRef<log::LoggingSystem> logging_system,
                          qtils::SharedRef<metrics::Metrics> metrics,
                          yaml::Node yaml,
                          std::string current_node_id);

    log::Logger logger_;
    qtils::SharedRef<metrics::Metrics> metrics_;
    std::string current_node_id_;
    std::unordered_map<ValidatorIndex, std::string> index_to_node_;
    std::unordered_map<std::string, std::vector<ValidatorIndex>>
        node_to_indices_;
    ValidatorIndices current_validator_indices_;
  };
}  // namespace lean
