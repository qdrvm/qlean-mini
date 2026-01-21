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

  soralog::Configurator::Result LoggingSystem::tuneLoggingSystem(
      const std::vector<std::string> &cfg) {
    auto default_group_set = false;

    soralog::Configurator::Result result;
    if (cfg.empty()) {
      return result;
    }

    for (auto &chunk : cfg) {
      if (chunk.empty()) {
        result.has_error = true;
        result.message += "E: Empty arg\n";
        continue;
      }

      std::istringstream iss1(chunk);
      std::string piece;

      while (std::getline(iss1, piece, ',')) {
        if (piece.empty()) {
          continue;
        }

        std::size_t pos = piece.find('=');
        if (pos == std::string::npos) {
          auto res = str2lvl(piece);
          if (res.has_value()) {
            auto level = res.value();
            if (default_group_set) {
              result.has_warning = true;
              result.message +=
                  "W: Level of default group was set several times; "
                  "last time to '"
                  + piece + "'\n";
            } else {
              result.message +=
                  "I: Level of default group was set to '" + piece + "'\n";
            }
            logging_system_->setLevelOfGroup(lean::log::defaultGroupName,
                                             level);
            default_group_set = true;
            continue;
          }
          result.has_error = true;
          result.message +=
              "E: Invalid level of default group: " + piece + "\n";
          continue;
        }

        std::istringstream iss2(piece);

        std::string group_name;
        std::getline(iss2, group_name, '=');

        std::string level_string;
        std::getline(iss2, level_string);

        if (not logging_system_->getGroup(group_name)) {
          result.has_warning = true;
          result.message += "W: Unknown group: " + group_name + "\n";
          continue;
        }

        auto res = str2lvl(level_string);
        if (not res.has_value()) {
          result.has_error = true;
          result.message += "E: Invalid level of group '" + group_name
                          + "': " + level_string + "\n";
          continue;
        }
        auto level = res.value();

        logging_system_->setLevelOfGroup(group_name, level);
      }
    }

    if (result.has_error or result.has_warning) {
      result.message =
          "I: Some problems are found during tuning of logging system by CLI "
          "args:\n"
          + result.message;
    } else {
      result.message.clear();
    }
    return result;
  }

}  // namespace lean::log
