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

#include <boost/algorithm/string/trim.hpp>
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
        ("base-path", po::value<std::string>(), "Set base path. All relative paths will be resolved based on this path.")
        ("config,c", po::value<std::string>(),  "Optional. Filepath to load configuration from. Overrides default configuration values.")
        ("genesis", po::value<std::string>(), "Set path to genesis config.yaml file.")
        ("modules-dir", po::value<std::string>(), "Set path to directory containing modules.")
        ("bootnodes", po::value<std::string>(), "Set path to nodes.yaml file containing boot node ENRs.")
        ("validator-registry-path",
         po::value<std::string>(),
         "Set path to validators.yaml file containing validator registry.")
        ("name,n", po::value<std::string>(), "Set name of node.")
        ("node-id", po::value<std::string>(), "Node id from validators.yaml")
        ("node-key", po::value<std::string>(), "Set secp256k1 node key as hex string (with or without 0x prefix).")
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
      std::println(std::cout, "Other commands:");
      std::println(std::cout, "  qlean key generate-node-key");
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
  static constexpr std::string_view default_logging_yaml = R"yaml(
sinks:
  - name: console
    type: console
    stream: stdout
    thread: name
    color: true
    latency: 0
groups:
  - name: main
    sink: console
    level: info
    is_fallback: true
    children:
      - name: lean
        children:
          - name: modules
            children:
              - name: example_module
              - name: synchronizer_module
              - name: networking_module
              - name: production_module
          - name: injector
          - name: application
          - name: rpc
          - name: metrics
          - name: threads
          - name: storage
            children:
              - name: block_storage
)yaml";

  outcome::result<YAML::Node> Configurator::getLoggingConfig() {
    auto load_default = [&]() -> outcome::result<YAML::Node> {
      try {
        return YAML::Load(std::string(default_logging_yaml));
      } catch (const std::exception &e) {
        file_errors_ << "E: Failed to load default logging config: " << e.what()
                     << "\n";
        return Error::ConfigFileParseFailed;
      }
    };

    if (not config_file_.has_value()) {
      return load_default();
    }
    auto logging = (*config_file_)["logging"];
    if (logging.IsDefined()) {
      return logging;
    }
    return load_default();
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
          auto node_id = section["node-id"];
          if (node_id.IsDefined()) {
            if (node_id.IsScalar()) {
              auto value = node_id.as<std::string>();
              config_->node_id_ = value;
            } else {
              file_errors_ << "E: Value 'general.node_id' must be scalar\n";
              file_has_error_ = true;
            }
          }
          auto node_key = section["node-key"];
          if (node_key.IsDefined()) {
            if (node_key.IsScalar()) {
              auto value = node_key.as<std::string>();
              boost::trim(value);
              if (value.empty()) {
                config_->node_key_hex_.reset();
              } else {
                config_->node_key_hex_ = value;
              }
            } else {
              file_errors_ << "E: Value 'general.node_key' must be scalar\n";
              file_has_error_ = true;
            }
          }
          auto base_path = section["base-path"];
          if (base_path.IsDefined()) {
            if (base_path.IsScalar()) {
              auto value = base_path.as<std::string>();
              config_->base_path_ = value;
            } else {
              file_errors_ << "E: Value 'general.base-path' must be scalar\n";
              file_has_error_ = true;
            }
          }
          auto genesis = section["genesis"];
          if (genesis.IsDefined()) {
            if (genesis.IsScalar()) {
              auto value = genesis.as<std::string>();
              config_->genesis_config_path_ = value;
            } else {
              file_errors_ << "E: Value 'general.genesis' must be scalar\n";
              file_has_error_ = true;
            }
          }
          auto modules_dir = section["modules-dir"];
          if (modules_dir.IsDefined()) {
            if (modules_dir.IsScalar()) {
              auto value = modules_dir.as<std::string>();
              config_->modules_dir_ = value;
            } else {
              file_errors_ << "E: Value 'general.modules_dir' must be scalar\n";
              file_has_error_ = true;
            }
          }
          auto bootnodes_file = section["bootnodes"];
          if (bootnodes_file.IsDefined()) {
            if (bootnodes_file.IsScalar()) {
              auto value = bootnodes_file.as<std::string>();
              config_->bootnodes_file_ = value;
            } else {
              file_errors_ << "E: Value 'general.bootnodes' must be scalar\n";
              file_has_error_ = true;
            }
          }
          auto validator_registry_path = section["validator-registry-path"];
          if (validator_registry_path.IsDefined()) {
            if (validator_registry_path.IsScalar()) {
              auto value = validator_registry_path.as<std::string>();
              config_->validator_registry_path_ = value;
            } else {
              file_errors_ << "E: Value 'general.validator_registry_path' must "
                              "be scalar\n";
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
        cli_values_map_, "node-id", [&](const std::string &value) {
          config_->node_id_ = value;
        });
    find_argument<std::string>(
        cli_values_map_, "node-key", [&](const std::string &value) {
          auto trimmed = value;
          boost::trim(trimmed);
          if (trimmed.empty()) {
            config_->node_key_hex_.reset();
          } else {
            config_->node_key_hex_ = trimmed;
          }
        });
    find_argument<std::string>(
        cli_values_map_, "base-path", [&](const std::string &value) {
          config_->base_path_ = value;
        });
    find_argument<std::string>(
        cli_values_map_, "modules-dir", [&](const std::string &value) {
          config_->modules_dir_ = value;
        });
    find_argument<std::string>(
        cli_values_map_, "genesis", [&](const std::string &value) {
          config_->genesis_config_path_ = value;
        });
    find_argument<std::string>(
        cli_values_map_, "bootnodes", [&](const std::string &value) {
          config_->bootnodes_file_ = value;
        });
    find_argument<std::string>(cli_values_map_,
                               "validator-registry-path",
                               [&](const std::string &value) {
                                 config_->validator_registry_path_ = value;
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

    if (not config_->bootnodes_file_.empty()) {
      config_->bootnodes_file_ = make_absolute(config_->bootnodes_file_);
      if (not is_regular_file(config_->bootnodes_file_)) {
        SL_ERROR(logger_,
                 "The 'bootnodes' file does not exist or is not a file: {}",
                 config_->bootnodes_file_.c_str());
        return Error::InvalidValue;
      }
    }

    if (not config_->validator_registry_path_.empty()) {
      config_->validator_registry_path_ =
          make_absolute(config_->validator_registry_path_);
      if (not is_regular_file(config_->validator_registry_path_)) {
        SL_ERROR(
            logger_,
            "The 'validator_registry_path' does not exist or is not a file: {}",
            config_->validator_registry_path_.c_str());
        return Error::InvalidValue;
      }
    }

    if (config_->genesis_config_path_.empty()) {
      SL_ERROR(logger_, "The 'genesis' path must be provided");
      return Error::InvalidValue;
    }

    config_->genesis_config_path_ =
        make_absolute(config_->genesis_config_path_);
    if (not is_regular_file(config_->genesis_config_path_)) {
      SL_ERROR(logger_,
               "The 'genesis' file does not exist or is not a file: {}",
               config_->genesis_config_path_.c_str());
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
