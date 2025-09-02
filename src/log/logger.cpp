/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "log/logger.hpp"

#include <iostream>

#include <boost/assert.hpp>
#include <qtils/enum_error_code.hpp>
#include <qtils/outcome.hpp>
#include <soralog/impl/sink_to_console.hpp>

OUTCOME_CPP_DEFINE_CATEGORY(lean::log, Error, e) {
  using E = lean::log::Error;
  switch (e) {
    case E::WRONG_LEVEL:
      return "Unknown level";
    case E::WRONG_GROUP:
      return "Unknown group";
    case E::WRONG_LOGGER:
      return "Unknown logger";
  }
  return "Unknown log::Error";
}

namespace lean::log {

  outcome::result<Level> str2lvl(std::string_view str) {
    if (str == "trace") {
      return Level::TRACE;
    }
    if (str == "debug") {
      return Level::DEBUG;
    }
    if (str == "verbose") {
      return Level::VERBOSE;
    }
    if (str == "info" or str == "inf") {
      return Level::INFO;
    }
    if (str == "warning" or str == "warn") {
      return Level::WARN;
    }
    if (str == "error" or str == "err") {
      return Level::ERROR;
    }
    if (str == "critical" or str == "crit") {
      return Level::CRITICAL;
    }
    if (str == "off" or str == "no") {
      return Level::OFF;
    }
    return Error::WRONG_LEVEL;
  }

  LoggingSystem::LoggingSystem(
      std::shared_ptr<soralog::LoggingSystem> logging_system)
      : logging_system_(logging_system) {}

  void LoggingSystem::tuneLoggingSystem(const std::vector<std::string> &cfg) {
    if (cfg.empty()) {
      return;
    }

    for (auto &chunk : cfg) {
      if (auto res = str2lvl(chunk); res.has_value()) {
        auto level = res.value();
        logging_system_->setLevelOfGroup(lean::log::defaultGroupName, level);
        continue;
      }

      std::istringstream iss2(chunk);

      std::string group_name;
      if (not std::getline(iss2, group_name, '=')) {
        std::cerr << "Can't read group";
      }
      if (not logging_system_->getGroup(group_name)) {
        std::cerr << "Unknown group: " << group_name
                  << std::endl;  // NOLINT(performance-avoid-endl)
        continue;
      }

      std::string level_string;
      if (not std::getline(iss2, level_string)) {
        std::cerr << "Can't read level for group '" << group_name << "'"
                  << std::endl;  // NOLINT(performance-avoid-endl)
        continue;
      }
      auto res = str2lvl(level_string);
      if (not res.has_value()) {
        std::cerr << "Invalid level: " << level_string
                  << std::endl;  // NOLINT(performance-avoid-endl)
        continue;
      }
      auto level = res.value();

      logging_system_->setLevelOfGroup(group_name, level);
    }
  }

}  // namespace lean::log
