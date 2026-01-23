/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <mutex>

#include <log/logger.hpp>

// #include <libp2p/log/configurator.hpp>

// #include <log/configurator.hpp>
#include <soralog/impl/configurator_from_yaml.hpp>

namespace testutil {

  // supposed to be called in SetUpTestCase
  inline qtils::SharedRef<lean::log::LoggingSystem> prepareLoggers(
      soralog::Level level = soralog::Level::INFO) {
    static qtils::SharedRef<lean::log::LoggingSystem> logging_system = ({
      auto testing_log_config = std::string(R"(
sinks:
  - name: console
    type: console
    capacity: 4
    latency: 0
groups:
  - name: main
    sink: console
    level: info
    is_fallback: true
    children:
      - name: testing
        level: trace
      - name: libp2p
        level: off
)");

      // Setup logging system
      auto log_config = YAML::Load(testing_log_config);
      if (not log_config.IsDefined()) {
        throw std::runtime_error("Log config is not defined");
      }

      auto log_configurator =
          std::make_shared<soralog::ConfiguratorFromYAML>(log_config);

      auto logging_system_ =
          std::make_shared<soralog::LoggingSystem>(std::move(log_configurator));

      auto config_result = logging_system_->configure();
      if (not config_result.message.empty()) {
        (config_result.has_error ? std::cerr : std::cout)
            << config_result.message << '\n';
      }
      if (config_result.has_error) {
        throw std::runtime_error("Cannot configure logging");
      }

      std::make_shared<lean::log::LoggingSystem>(std::move(logging_system_));
    });

    std::ignore =
        logging_system->setLevelOfGroup(lean::log::defaultGroupName, level);

    return logging_system;
  }

}  // namespace testutil
