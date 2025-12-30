/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <chrono>
#include <iostream>
#include <memory>
#include <system_error>

#include <fmt/format.h>
#include <qtils/final_action.hpp>
#include <soralog/impl/configurator_from_yaml.hpp>
#include <soralog/logging_system.hpp>

#include "app/application.hpp"
#include "app/configuration.hpp"
#include "app/configurator.hpp"
#include "commands/generate_genesis.hpp"
#include "commands/key_generate_node_key.hpp"
#include "injector/node_injector.hpp"
#include "loaders/loader.hpp"
#include "log/logger.hpp"
#include "modules/module_loader.hpp"
#include "se/subscription.hpp"
#include "types/config.hpp"

using std::string_view_literals::operator""sv;

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)

namespace {
  void wrong_usage() {
    std::cerr << "Wrong usage.\n"
                 "Run with `--help' argument to print usage\n";
  }

  using lean::app::Application;
  using lean::app::Configuration;
  using lean::injector::NodeInjector;
  using lean::log::LoggingSystem;

  int run_node(std::shared_ptr<LoggingSystem> logsys,
               std::shared_ptr<Configuration> appcfg) {
    auto injector = std::make_unique<NodeInjector>(logsys, appcfg);

    // Load modules
    std::deque<std::unique_ptr<lean::loaders::Loader>> loaders;
    {
      auto logger = logsys->getLogger("Modules", "lean");
      const std::string path(appcfg->modulesDir());

      lean::modules::ModuleLoader module_loader(path);
      auto modules_res = module_loader.get_modules();
      if (modules_res.has_error()) {
        SL_CRITICAL(logger, "Failed to load modules from path: {}", path);
        return EXIT_FAILURE;
      }
      auto &modules = modules_res.value();

      for (const auto &module : modules) {
        SL_INFO(logger,
                "Found module '{}', path: {}",
                module->get_module_info(),
                module->get_path());

        auto loader = injector->register_loader(module);

        // Skip unsupported
        if (not loader) {
          SL_WARN(logger,
                  "Module '{}' has unsupported loader '{}'; Skipped",
                  module->get_module_info(),
                  module->get_loader_id());
          continue;
        }

        // Init module
        SL_INFO(logger,
                "Module '{}' loaded by '{}'",
                module->get_module_info(),
                module->get_loader_id());
        loaders.emplace_back(std::move(loader));
      }

      // Notify about all modules are loaded
      // se_manager->notify(lean::EventTypes::LoadingIsFinished);
    }

    auto logger = logsys->getLogger("Main", lean::log::defaultGroupName);
    auto app = injector->injectApplication();
    SL_INFO(logger, "Node started. Version: {} ", appcfg->nodeVersion());

    app->run();

    SL_INFO(logger, "Node stopped");
    logger->flush();

    return EXIT_SUCCESS;
  }

}  // namespace

int main(int argc, const char **argv, const char **env) {
  setlinebuf(stdout);
  setlinebuf(stderr);

  soralog::util::setThreadName("lean-node");

  auto getArg = [&](size_t i) {
    return static_cast<ptrdiff_t>(i) < argc
             ? std::make_optional(std::string_view{argv[i]})
             : std::nullopt;
  };

  qtils::FinalAction flush_std_streams_at_exit([] {
    std::cout.flush();
    std::cerr.flush();
  });

  if (argc == 0) {
    // Abnormal run
    wrong_usage();
    return EXIT_FAILURE;
  }

  if (argc == 1) {
    // Run without arguments
    wrong_usage();
    return EXIT_FAILURE;
  }

  if (getArg(1) == "key" and getArg(2) == "generate-node-key") {
    cmdKeyGenerateNodeKey();
    return EXIT_SUCCESS;
  }
  if (getArg(1) == "generate-genesis") {
    return cmdGenerateGenesis(getArg);
  }

  auto app_configurator =
      std::make_unique<lean::app::Configurator>(argc, argv, env);

  // Parse CLI args for help, version and config
  if (auto res = app_configurator->step1(); res.has_value()) {
    if (res.value()) {
      return EXIT_SUCCESS;
    }
  } else {
    return EXIT_FAILURE;
  }

  // Parse remaining args
  if (auto res = app_configurator->step2(); res.has_value()) {
    if (res.value()) {
      return EXIT_SUCCESS;
    }
  } else {
    return EXIT_FAILURE;
  }

  // Setup logging system
  auto logging_system = ({
    auto log_config = app_configurator->getLoggingConfig();
    if (log_config.has_error()) {
      std::cerr << "Logging config is empty.\n";
      return EXIT_FAILURE;
    }

    auto log_configurator = std::make_shared<soralog::ConfiguratorFromYAML>(
        std::shared_ptr<soralog::Configurator>(nullptr), log_config.value());

    auto logging_system =
        std::make_shared<soralog::LoggingSystem>(std::move(log_configurator));

    auto config_result = logging_system->configure();
    if (not config_result.message.empty()) {
      (config_result.has_error ? std::cerr : std::cout)
          << config_result.message << '\n';
    }
    if (config_result.has_error) {
      return EXIT_FAILURE;
    }

    std::make_shared<lean::log::LoggingSystem>(std::move(logging_system));
  });


  // Setup config
  auto app_configuration = ({
    auto logger = logging_system->getLogger("Configurator", "lean");

    auto config_res = app_configurator->calculateConfig(logger);
    if (config_res.has_error()) {
      auto error = config_res.error();
      SL_CRITICAL(logger, "Failed to calculate config: {}", error);
      fmt::println(std::cerr, "Failed to calculate config: {}", error);
      fmt::println(std::cerr, "See more details in the log");
      return EXIT_FAILURE;
    }

    config_res.value();
  });

  int exit_code;
  auto logger = logging_system->getLogger("Main", lean::log::defaultGroupName);
  {
    std::string_view name{argv[1]};

    if (name.substr(0, 1) == "-") {
      // The first argument isn't subcommand, run as node
      exit_code = run_node(logging_system, app_configuration);
    }

    // else if (false and name == "subcommand-1"s) {
    //   exit_code = execute_subcommend_1(argc - 1, argv + 1);
    // }
    //
    // else if (false and name == "subcommand-2"s) {
    //   exit_code = execute_subcommend_2(argc - 1, argv + 1);
    // }

    else {
      // No subcommand, but argument is not a valid option: begins not with dash
      wrong_usage();
      return EXIT_FAILURE;
    }
  }

  SL_INFO(logger, "All components are stopped");
  logger->flush();

  return exit_code;
}

// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
