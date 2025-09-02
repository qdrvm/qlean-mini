/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app/configurator.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <qtils/outcome.hpp>
#include <qtils/shared_ref.hpp>

#include "app/build_version.hpp"
#include "app/configuration.hpp"
#include "utils/parsers.hpp"

using Endpoint = boost::asio::ip::tcp::endpoint;

OUTCOME_CPP_DEFINE_CATEGORY(lean::app, Configurator::Error, e) {
  using E = lean::app::Configurator::Error;
  switch (e) {
    case E::CliArgsParseFailed:
      return "CLI Arguments parse failed";
    case E::ConfigFileParseFailed:
      return "Config file parse failed";
    case E::InvalidValue:
      return "Result config has invalid values";
  }
  BOOST_UNREACHABLE_RETURN("Unknown log::Error");
}

namespace {
  template <typename T, typename Func>
  void find_argument(boost::program_options::variables_map &vm,
                     const char *name,
                     Func &&f) {
    assert(nullptr != name);
    if (auto it = vm.find(name); it != vm.end()) {
      if (it->second.defaulted()) {
        return;
      }
      std::forward<Func>(f)(it->second.as<T>());
    }
  }

  template <typename T>
  std::optional<T> find_argument(boost::program_options::variables_map &vm,
                                 const std::string &name) {
    if (auto it = vm.find(name); it != vm.end()) {
      if (!it->second.defaulted()) {
        return it->second.as<T>();
      }
    }
    return std::nullopt;
  }

  bool find_argument(boost::program_options::variables_map &vm,
                     const std::string &name) {
    if (auto it = vm.find(name); it != vm.end()) {
      if (!it->second.defaulted()) {
        return true;
      }
    }
    return false;
  }

}  // namespace

namespace lean::app {

  Configurator::Configurator(int argc, const char **argv, const char **env)
      : argc_(argc), argv_(argv), env_(env) {
    config_ = std::make_shared<Configuration>();

    config_->version_ = buildVersion();
    config_->name_ = "noname";

    config_->database_.directory = "db";
    config_->database_.cache_size = 512 << 20;  // 512MiB

    config_->metrics_.endpoint = {boost::asio::ip::address_v4::any(), 9615};
    config_->metrics_.enabled = std::nullopt;

    namespace po = boost::program_options;

    // clang-format off

    po::options_description general_options("General options", 120, 100);
    general_options.add_options()
        ("help,h", "Show this help message.")
        ("version,v", "Show version information.")
        ("base_path", po::value<std::string>(), "Set base path. All relative paths will be resolved based on this path.")
        ("config,c", po::value<std::string>(),  "Optional. Filepath to load configuration from. Overrides default configuration values.")
        ("spec_file", po::value<std::string>(), "Set path to spec file.")
        ("modules_dir", po::value<std::string>(), "Set path to directory containing modules.")
        ("name,n", po::value<std::string>(), "Set name of node.")
        ("log,l", po::value<std::vector<std::string>>(),
          "Sets a custom logging filter.\n"
          "Syntax: <target>=<level>, e.g., -llibp2p=off.\n"
          "Log levels: trace, debug, verbose, info, warn, error, critical, off.\n"
          "Default: all targets log at `info`.\n"
          "Global log level can be set with: -l<level>.")
        ;

    po::options_description storage_options("Storage options");
    storage_options.add_options()
        ("db_path", po::value<std::string>()->default_value(config_->database_.directory), "Path to DB directory. Can be relative on base path.")
        // ("db-tmp", "Use temporary storage path.")
        ("db_cache_size", po::value<uint32_t>()->default_value(config_->database_.cache_size), "Limit the memory the database cache can use <MiB>.")
        ;

    po::options_description metrics_options("Metric options");
    metrics_options.add_options()
        ("prometheus_disable", "Set to disable OpenMetrics.")
        ("prometheus_host", po::value<std::string>(), "Set address for OpenMetrics over HTTP.")
        ("prometheus_port", po::value<uint16_t>(), "Set port for OpenMetrics over HTTP.")
        ;

    // clang-format on

    cli_options_
        .add(general_options)  //
        .add(storage_options)
        .add(metrics_options);
  }

  outcome::result<bool> Configurator::step1() {  // read min cli-args and config
    namespace po = boost::program_options;
    namespace fs = std::filesystem;

    po::options_description options;
    options.add_options()("help,h", "show help")("version,v", "show version")(
        "config,c", po::value<std::string>(), "config-file path");

    po::variables_map vm;

    // first-run parse to read-only general options and to lookup for "help",
    // "config" and "version". all the rest options are ignored
    try {
      po::parsed_options parsed = po::command_line_parser(argc_, argv_)
                                      .options(options)
                                      .allow_unregistered()
                                      .run();
      po::store(parsed, vm);
      po::notify(vm);
    } catch (const std::exception &e) {
      std::cerr << "Error: " << e.what() << '\n'
                << "Try run with option '--help' for more information\n";
      return Error::CliArgsParseFailed;
    }

    if (vm.contains("help")) {
      std::cout << "Lean-node version " << buildVersion() << '\n';
      std::cout << cli_options_ << '\n';
      return true;
    }

    if (vm.contains("version")) {
      std::cout << "Lean-node version " << buildVersion() << '\n';
      return true;
    }

    if (vm.contains("config")) {
      auto path = vm["config"].as<std::string>();
      try {
        config_file_ = YAML::LoadFile(path);
      } catch (const std::exception &exception) {
        std::cerr << "Error: Can't parse file "
                  << std::filesystem::weakly_canonical(path) << ": "
                  << exception.what() << "\n"
                  << "Option --config must be path to correct yaml-file\n"
                  << "Try run with option '--help' for more information\n";
        return Error::ConfigFileParseFailed;
      }
    }

    return false;
  }

  outcome::result<bool> Configurator::step2() {
    namespace po = boost::program_options;
    namespace fs = std::filesystem;

    try {
      // second-run parse to gather all known options
      // with reporting about any unrecognized input
      po::parsed_options parsed =
          po::command_line_parser(argc_, argv_).options(cli_options_).run();
      po::store(parsed, cli_values_map_);
      po::notify(cli_values_map_);
    } catch (const std::exception &e) {
      std::cerr << "Error: " << e.what() << '\n'
                << "Try run with option '--help' for more information\n";
      return Error::CliArgsParseFailed;
    }

    return false;
  }

  outcome::result<YAML::Node> Configurator::getLoggingConfig() {
    auto logging = (*config_file_)["logging"];
    if (logging.IsDefined()) {
      return logging;
    }
    return YAML::Node{};  // TODO return default logging config
  }

  outcome::result<std::shared_ptr<Configuration>> Configurator::calculateConfig(
      qtils::SharedRef<soralog::Logger> logger) {
    logger_ = std::move(logger);
    OUTCOME_TRY(initGeneralConfig());
    OUTCOME_TRY(initDatabaseConfig());
    OUTCOME_TRY(initOpenMetricsConfig());

    return config_;
  }

  outcome::result<void> Configurator::initGeneralConfig() {
    // Init by config-file
    if (config_file_.has_value()) {
      auto section = (*config_file_)["general"];
      if (section.IsDefined()) {
        if (section.IsMap()) {
          auto name = section["name"];
          if (name.IsDefined()) {
            if (name.IsScalar()) {
              auto value = name.as<std::string>();
              config_->name_ = value;
            } else {
              file_errors_ << "E: Value 'general.name' must be scalar\n";
              file_has_error_ = true;
            }
          }
          auto base_path = section["base_path"];
          if (base_path.IsDefined()) {
            if (base_path.IsScalar()) {
              auto value = base_path.as<std::string>();
              config_->base_path_ = value;
            } else {
              file_errors_ << "E: Value 'general.base_path' must be scalar\n";
              file_has_error_ = true;
            }
          }
          auto spec_file = section["spec_file"];
          if (spec_file.IsDefined()) {
            if (spec_file.IsScalar()) {
              auto value = spec_file.as<std::string>();
              config_->spec_file_ = value;
            } else {
              file_errors_ << "E: Value 'general.spec_file' must be scalar\n";
              file_has_error_ = true;
            }
          }
          auto modules_dir = section["modules_dir"];
          if (modules_dir.IsDefined()) {
            if (modules_dir.IsScalar()) {
              auto value = modules_dir.as<std::string>();
              config_->modules_dir_ = value;
            } else {
              file_errors_ << "E: Value 'general.modules_dir' must be scalar\n";
              file_has_error_ = true;
            }
          }
        } else {
          file_errors_ << "E: Section 'general' defined, but is not map\n";
          file_has_error_ = true;
        }
      }
    }

    if (file_has_error_) {
      std::string path;
      find_argument<std::string>(
          cli_values_map_, "config", [&](const std::string &value) {
            path = value;
          });
      SL_ERROR(logger_, "Config file `{}` has some problems:", path);
      std::istringstream iss(file_errors_.str());
      std::string line;
      while (std::getline(iss, line)) {
        SL_ERROR(logger_, "  {}", std::string_view(line).substr(3));
      }
      return Error::ConfigFileParseFailed;
    }

    // Adjust by CLI arguments
    bool fail;

    fail = false;
    find_argument<std::string>(
        cli_values_map_, "name", [&](const std::string &value) {
          config_->name_ = value;
        });
    find_argument<std::string>(
        cli_values_map_, "base_path", [&](const std::string &value) {
          config_->base_path_ = value;
        });
    find_argument<std::string>(
        cli_values_map_, "modules_dir", [&](const std::string &value) {
          config_->modules_dir_ = value;
        });
    find_argument<std::string>(
        cli_values_map_, "spec_file", [&](const std::string &value) {
          config_->spec_file_ = value;
        });
    if (fail) {
      return Error::CliArgsParseFailed;
    }

    // Check values
    if (not config_->base_path_.is_absolute()) {
      SL_ERROR(logger_,
               "The 'base_path' must be defined as absolute: {}",
               config_->base_path_.c_str());
      return Error::InvalidValue;
    }
    if (not is_directory(config_->base_path_)) {
      SL_ERROR(logger_,
               "The 'base_path' does not exist or is not a directory: {}",
               config_->base_path_.c_str());
      return Error::InvalidValue;
    }
    current_path(config_->base_path_);

    auto make_absolute = [&](const std::filesystem::path &path) {
      return weakly_canonical(config_->base_path_.is_absolute()
                                  ? path
                                  : (config_->base_path_ / path));
    };

    config_->modules_dir_ = make_absolute(config_->modules_dir_);
    if (not is_directory(config_->modules_dir_)) {
      SL_ERROR(logger_,
               "The 'modules_dir' does not exist or is not a directory: {}",
               config_->modules_dir_.c_str());
      return Error::InvalidValue;
    }

    config_->spec_file_ = make_absolute(config_->spec_file_);
    if (not is_regular_file(config_->spec_file_)) {
      SL_ERROR(logger_,
               "The 'spec_file' does not exist or is not a file: {}",
               config_->spec_file_.c_str());
      return Error::InvalidValue;
    }

    return outcome::success();
  }

  outcome::result<void> Configurator::initDatabaseConfig() {
    // Init by config-file
    if (config_file_.has_value()) {
      auto section = (*config_file_)["database"];
      if (section.IsDefined()) {
        if (section.IsMap()) {
          auto path = section["path"];
          if (path.IsDefined()) {
            if (path.IsScalar()) {
              auto value = path.as<std::string>();
              config_->database_.directory = value;
            } else {
              file_errors_ << "E: Value 'database.path' must be scalar\n";
              file_has_error_ = true;
            }
          }
          auto spec_file = section["cache_size"];
          if (spec_file.IsDefined()) {
            if (spec_file.IsScalar()) {
              auto value = util::parseByteQuantity(spec_file.as<std::string>());
              if (value.has_value()) {
                config_->database_.cache_size = value.value();
              } else {
                file_errors_ << "E: Bad 'cache_size' value; "
                                "Expected: 4096, 512Mb, 1G, etc.\n";
              }
            } else {
              file_errors_ << "E: Value 'database.cache_size' must be scalar\n";
              file_has_error_ = true;
            }
          }
        } else {
          file_errors_ << "E: Section 'database' defined, but is not map\n";
          file_has_error_ = true;
        }
      }
    }

    if (file_has_error_) {
      std::string path;
      find_argument<std::string>(
          cli_values_map_, "config", [&](const std::string &value) {
            path = value;
          });
      SL_ERROR(logger_, "Config file `{}` has some problems:", path);
      std::istringstream iss(file_errors_.str());
      std::string line;
      while (std::getline(iss, line)) {
        SL_ERROR(logger_, "  {}", std::string_view(line).substr(3));
      }
      return Error::ConfigFileParseFailed;
    }

    // Adjust by CLI arguments
    bool fail;

    fail = false;
    find_argument<std::string>(
        cli_values_map_, "db_path", [&](const std::string &value) {
          config_->database_.directory = value;
        });
    find_argument<uint32_t>(
        cli_values_map_, "db_cache_size", [&](const uint32_t &value) {
          config_->database_.cache_size = value;
        });
    if (fail) {
      return Error::CliArgsParseFailed;
    }

    // Check values
    auto make_absolute = [&](const std::filesystem::path &path) {
      return weakly_canonical(config_->base_path_.is_absolute()
                                  ? path
                                  : (config_->base_path_ / path));
    };

    config_->database_.directory = make_absolute(config_->database_.directory);

    return outcome::success();
  }

  outcome::result<void> Configurator::initOpenMetricsConfig() {
    if (config_file_.has_value()) {
      auto section = (*config_file_)["metrics"];
      if (section.IsDefined()) {
        if (section.IsMap()) {
          auto enabled = section["enabled"];
          if (enabled.IsDefined()) {
            if (enabled.IsScalar()) {
              auto value = enabled.as<std::string>();
              if (value == "true") {
                config_->metrics_.enabled = true;
              } else if (value == "false") {
                config_->metrics_.enabled = false;
              } else {
                file_errors_
                    << "E: Value 'network.metrics.enabled' has wrong value. "
                       "Expected 'true' or 'false'\n";
                file_has_error_ = true;
              }
            } else {
              file_errors_ << "E: Value 'metrics.enabled' must be scalar\n";
              file_has_error_ = true;
            }
          }

          auto host = section["host"];
          if (host.IsDefined()) {
            if (host.IsScalar()) {
              auto value = host.as<std::string>();
              boost::beast::error_code ec;
              auto address = boost::asio::ip::make_address(value, ec);
              if (!ec) {
                config_->metrics_.endpoint = {
                    address, config_->metrics_.endpoint.port()};
                if (not config_->metrics_.enabled.has_value()) {
                  config_->metrics_.enabled = true;
                }
              } else {
                file_errors_ << "E: Value 'network.metrics.host' defined, "
                                "but has invalid value\n";
              }
            } else {
              file_errors_ << "E: Value 'network.metrics.host' defined, "
                              "but is not scalar\n";
              file_has_error_ = true;
            }
          }

          auto port = section["port"];
          if (port.IsDefined()) {
            if (port.IsScalar()) {
              auto value = port.as<ssize_t>();
              if (value > 0 and value <= 65535) {
                config_->metrics_.endpoint = {
                    config_->metrics_.endpoint.address(),
                    static_cast<uint16_t>(value)};
                if (not config_->metrics_.enabled.has_value()) {
                  config_->metrics_.enabled = true;
                }
              } else {
                file_errors_ << "E: Value 'network.metrics.port' defined, "
                                "but has invalid value\n";
                file_has_error_ = true;
              }
            } else {
              file_errors_ << "E: Value 'network.metrics.port' defined, "
                              "but is not scalar\n";
              file_has_error_ = true;
            }
          }

        } else {
          file_errors_ << "E: Section 'metrics' defined, but is not map\n";
          file_has_error_ = true;
        }
      }
    }

    bool fail;

    fail = false;
    find_argument<std::string>(
        cli_values_map_, "prometheus_host", [&](const std::string &value) {
          boost::beast::error_code ec;
          auto address = boost::asio::ip::make_address(value, ec);
          if (!ec) {
            config_->metrics_.endpoint = {address,
                                          config_->metrics_.endpoint.port()};
            if (not config_->metrics_.enabled.has_value()) {
              config_->metrics_.enabled = true;
            }
          } else {
            std::cerr << "Option --prometheus_host has invalid value\n"
                      << "Try run with option '--help' for more information\n";
            fail = true;
          }
        });
    if (fail) {
      return Error::CliArgsParseFailed;
    }

    fail = false;
    find_argument<uint16_t>(
        cli_values_map_, "prometheus_port", [&](const uint16_t &value) {
          if (value > 0 and value <= 65535) {
            config_->metrics_.endpoint = {config_->metrics_.endpoint.address(),
                                          static_cast<uint16_t>(value)};
            if (not config_->metrics_.enabled.has_value()) {
              config_->metrics_.enabled = true;
            }
          } else {
            std::cerr << "Option --prometheus_port has invalid value\n"
                      << "Try run with option '--help' for more information\n";
            fail = true;
          }
        });
    if (fail) {
      return Error::CliArgsParseFailed;
    }

    if (find_argument(cli_values_map_, "prometheus_disabled")) {
      config_->metrics_.enabled = false;
    };
    if (not config_->metrics_.enabled.has_value()) {
      config_->metrics_.enabled = false;
    }

    return outcome::success();
  }

}  // namespace lean::app
