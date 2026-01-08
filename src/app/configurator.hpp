/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <boost/program_options.hpp>
#include <log/logger.hpp>
#include <qtils/enum_error_code.hpp>
#include <qtils/outcome.hpp>
#include <qtils/shared_ref.hpp>
#include <yaml-cpp/yaml.h>

#include "injector/dont_inject.hpp"

namespace soralog {
  class Logger;
}  // namespace soralog

namespace lean::app {
  class Configuration;
}  // namespace lean::app

namespace lean::app {

  class Configurator final {
   public:
    enum class Error : uint8_t {
      CliArgsParseFailed,
      ConfigFileParseFailed,
      InvalidValue,
    };

    DONT_INJECT(Configurator);

    Configurator() = delete;
    Configurator(Configurator &&) noexcept = delete;
    Configurator(const Configurator &) = delete;
    ~Configurator() = default;
    Configurator &operator=(Configurator &&) noexcept = delete;
    Configurator &operator=(const Configurator &) = delete;

    Configurator(int argc, const char **argv, const char **env);

    // Parse CLI args for help, version and config
    outcome::result<bool> step1();

    // Parse remaining CLI args
    outcome::result<bool> step2();

    outcome::result<YAML::Node> getLoggingConfig();
    std::vector<std::string> getLoggingCliArgs() {
      return logger_cli_args_;
    }

    outcome::result<std::shared_ptr<Configuration>> calculateConfig(
        qtils::SharedRef<soralog::Logger> logger);

   private:
    outcome::result<void> initGeneralConfig();
    outcome::result<void> initDatabaseConfig();
    outcome::result<void> initOpenMetricsConfig();

    int argc_;
    const char **argv_;
    const char **env_;

    std::shared_ptr<Configuration> config_;
    std::shared_ptr<soralog::Logger> logger_;

    std::optional<YAML::Node> config_file_;
    bool file_has_warn_ = false;
    bool file_has_error_ = false;
    std::ostringstream file_errors_;
    std::vector<std::string> logger_cli_args_;

    boost::program_options::options_description cli_options_;
    boost::program_options::variables_map cli_values_map_;
  };

}  // namespace lean::app

OUTCOME_HPP_DECLARE_ERROR(lean::app, Configurator::Error);
