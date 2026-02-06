/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
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
#include <fmt/format.h>
#include <qtils/outcome.hpp>
#include <qtils/shared_ref.hpp>

#include "app/build_version.hpp"
#include "app/configuration.hpp"
#include "app/default_config.hpp"
#include "app/validator_keys_manifest.hpp"
#include "crypto/xmss/xmss_provider_fake.hpp"
#include "crypto/xmss/xmss_util.hpp"
#include "executable/qlean_enable_shadow.hpp"
#include "log/formatters/filepath.hpp"
#include "modules/networking/get_node_key.hpp"
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
        ("data-dir", po::value<std::string>(), "Alias for \"--base-path\".")
        ("config,c", po::value<std::string>(),  "Optional. Filepath to load configuration from. Overrides default configuration values.")
        ("genesis", po::value<std::string>(), "Set path to genesis config yaml file (genesis/config.yaml).")
        ("listen-addr", po::value<std::string>(), "Set libp2p listen multiaddress.")
        ("modules-dir", po::value<std::string>(), "Set path to directory containing modules.")
        ("bootnodes", po::value<std::string>(), "Set path to yaml file containing boot node ENRs (genesis/nodes.yaml).")
        ("state-sync-url", po::value<std::string>(),  "Optional. URL for pre-syncing the state at startup if any")
        ("validator-registry-path", po::value<std::string>(), "Set path to yaml file containing validator registry (genesis/validators.yaml).")
        ("name,n", po::value<std::string>(), "Set name of node.")
        ("node-id", po::value<std::string>(), "Node id from validator registry (genesis/validators.yaml).")
        ("node-key", po::value<std::string>(), "Set secp256k1 node key as hex string (with or without 0x prefix).")
        ("xmss-pk", po::value<std::string>(), "Path to XMSS public key JSON file (required).")
        ("xmss-sk", po::value<std::string>(), "Path to XMSS secret key JSON file (required).")
        ("validator-keys-manifest", po::value<std::string>(), "Set path to yaml file containing validator keys manifest (required).")
        ("max-bootnodes", po::value<size_t>(), "Max bootnodes count to connect to.")
        ("log,l", po::value<std::vector<std::string>>(),
          "Sets a custom logging filter.\n"
          "Syntax: <target>=<level>, e.g., -llibp2p=off.\n"
          "Log levels: trace, debug, verbose, info, warn, error, critical, off.\n"
          "Default: all targets log at `info`.\n"
          "Global log level can be set with: -l<level>.")
          ;

    if constexpr (QLEAN_ENABLE_SHADOW) {
      general_options.add_options()
          ("shadow-xmss-aggregate-signatures-rate", "How many signatures can be aggregated per second (fake xmss provider)")
          ("shadow-xmss-verify-aggregated-signatures-rate", "How many signatures inside aggregated signature can be verified per second (fake xmss provider)")
          ;
    }

    po::options_description storage_options("Storage options");
    storage_options.add_options()
        ("db_path", po::value<std::string>()->default_value(config_->database_.directory), "Path to DB directory. Can be relative on base path.")
        // ("db-tmp", "Use temporary storage path.")
        ("db_cache_size", po::value<uint32_t>()->default_value(config_->database_.cache_size), "Limit the memory the database cache can use <MiB>.")
        ;

    po::options_description metrics_options("Metric options");
    metrics_options.add_options()
        ("metrics-disable", "Set to disable OpenMetrics.")
        ("metrics-host", po::value<std::string>(), "Set address for OpenMetrics over HTTP.")
        ("metrics-port", po::value<uint16_t>(), "Set port for OpenMetrics over HTTP.")
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

    // clang-format off

    po::options_description options;
    options.add_options()
        ("help,h", "show help")
        ("version,v", "show version")
        ("config,c", po::value<std::string>(), "config-file path")
        ("log,l", po::value<std::vector<std::string>>(), "Sets a custom logging filter")
        ;

    // clang-format on

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
      auto exe = std::filesystem::path{argv_[0]}.filename().string();
      std::cout << "Lean-node version " << buildVersion() << '\n';
      std::cout << cli_options_ << '\n';
      fmt::println("Other commands:");
      fmt::println("  {} key generate-node-key", exe);
      fmt::println("  {} generate-genesis", exe);
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

    if (vm.contains("log")) {
      logger_cli_args_ = vm["log"].as<std::vector<std::string>>();
    }

    return false;
  }

  outcome::result<bool> Configurator::step2() {
    namespace po = boost::program_options;
    namespace fs = std::filesystem;

    // clang-format off
    po::options_description metrics_options("Hidden options");
    metrics_options.add_options()
        ("prometheus-disable", "Set to disable OpenMetrics.")
        ("prometheus-host", po::value<std::string>(), "Set address for OpenMetrics over HTTP.")
        ("prometheus-port", po::value<uint16_t>(), "Set port for OpenMetrics over HTTP.")
        ;
    cli_options_.add(metrics_options);
    // clang-format on

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
    auto load_default = [&]() -> outcome::result<YAML::Node> {
      try {
        return YAML::Load(defaultConfigYaml())["logging"];
      } catch (const std::exception &e) {
        file_errors_ << "E: Failed to load embedded default config: "
                     << e.what() << "\n";
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
              if (auto r = keyPairFromPrivateKeyHex(value)) {
                config_->node_key_ = r.value();
              } else {
                fmt::println(file_errors_,
                             "E: Value 'general.node_key' must be private key "
                             "hex or it's file path: {}",
                             r.error());
                file_has_error_ = true;
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
          auto listen_addr = section["listen-addr"];
          if (listen_addr.IsDefined()) {
            if (listen_addr.IsScalar()) {
              auto value = listen_addr.as<std::string>();
              boost::trim(value);
              if (auto r = libp2p::Multiaddress::create(value)) {
                config_->listen_multiaddr_ = r.value();
              } else {
                fmt::println(file_errors_,
                             "E: Value 'general.listen_addr' must be valid "
                             "multiaddress: {}",
                             r.error());
                file_has_error_ = true;
              }
            } else {
              file_errors_ << "E: Value 'general.listen_addr' must be scalar\n";
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
          auto xmss_pk = section["xmss-pk"];
          if (xmss_pk.IsDefined()) {
            if (xmss_pk.IsScalar()) {
              auto value = xmss_pk.as<std::string>();
              config_->xmss_public_key_path_ = value;
            } else {
              file_errors_ << "E: Value 'general.xmss-pk' must be scalar\n";
              file_has_error_ = true;
            }
          }
          auto xmss_sk = section["xmss-sk"];
          if (xmss_sk.IsDefined()) {
            if (xmss_sk.IsScalar()) {
              auto value = xmss_sk.as<std::string>();
              config_->xmss_secret_key_path_ = value;
            } else {
              file_errors_ << "E: Value 'general.xmss-sk' must be scalar\n";
              file_has_error_ = true;
            }
          }
          auto validator_keys_manifest = section["validator-keys-manifest"];
          if (validator_keys_manifest.IsDefined()) {
            if (validator_keys_manifest.IsScalar()) {
              auto value = validator_keys_manifest.as<std::string>();
              config_->validator_keys_manifest_path_ = value;
            } else {
              file_errors_ << "E: Value 'general.validator-keys-manifest' must "
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
        cli_values_map_, "node-key", [&](std::string value) {
          boost::trim(value);
          if (auto r = keyPairFromPrivateKeyHex(value)) {
            config_->node_key_ = r.value();
          } else {
            SL_ERROR(logger_,
                     "'node-key' must be private key hex or it's file path: {}",
                     r.error());
            fail = true;
          }
        });
    if (auto max_bootnodes =
            find_argument<size_t>(cli_values_map_, "max-bootnodes")) {
      config_->max_bootnodes_ = *max_bootnodes;
    }
    find_argument<std::string>(
        cli_values_map_, "base-path", [&](const std::string &value) {
          config_->base_path_ = value;
        });
    find_argument<std::string>(
        cli_values_map_, "data-dir", [&](const std::string &value) {
          config_->base_path_ = value;
        });
    find_argument<std::string>(
        cli_values_map_, "modules-dir", [&](const std::string &value) {
          config_->modules_dir_ = value;
        });
    find_argument<std::string>(
        cli_values_map_, "listen-addr", [&](std::string value) {
          boost::trim(value);
          if (auto r = libp2p::Multiaddress::create(value)) {
            config_->listen_multiaddr_ = r.value();
          } else {
            SL_ERROR(logger_,
                     "'listen-addr' must be valid multiaddress: {}",
                     r.error());
            fail = true;
          }
        });
    find_argument<std::string>(
        cli_values_map_, "genesis", [&](const std::string &value) {
          config_->genesis_config_path_ = value;
        });
    find_argument<std::string>(
        cli_values_map_, "bootnodes", [&](const std::string &value) {
          config_->bootnodes_file_ = value;
        });
    find_argument<std::string>(
        cli_values_map_, "state-sync-url", [&](const std::string &value) {
          config_->state_sync_url_.emplace(value);
        });
    find_argument<std::string>(cli_values_map_,
                               "validator-registry-path",
                               [&](const std::string &value) {
                                 config_->validator_registry_path_ = value;
                               });
    find_argument<std::string>(
        cli_values_map_, "xmss-pk", [&](const std::string &value) {
          config_->xmss_public_key_path_ = value;
        });
    find_argument<std::string>(
        cli_values_map_, "xmss-sk", [&](const std::string &value) {
          config_->xmss_secret_key_path_ = value;
        });
    find_argument<std::string>(cli_values_map_,
                               "validator-keys-manifest",
                               [&](const std::string &value) {
                                 config_->validator_keys_manifest_path_ = value;
                               });
    if (fail) {
      return Error::CliArgsParseFailed;
    }

    // Resolve base_path_ to an absolute path (relative to config file dir or
    // CWD)
    {
      std::filesystem::path resolved = config_->base_path_;
      if (not resolved.is_absolute()) {
        if (auto cfg = find_argument<std::string>(cli_values_map_, "config");
            cfg.has_value()) {
          resolved = weakly_canonical(std::filesystem::path(*cfg).parent_path()
                                      / resolved);
        } else {
          resolved =
              weakly_canonical(std::filesystem::current_path() / resolved);
        }
      } else {
        resolved = weakly_canonical(resolved);
      }
      config_->base_path_ = std::move(resolved);
    }

    std::error_code ec;

    // If the base path doesn't exist -> try to create it (including parents)
    if (not exists(config_->base_path_, ec)) {
      if (ec) {
        SL_ERROR(logger_,
                 "Failed to check existence of 'base_path': {} ({}: {})",
                 config_->base_path_,
                 ec.value(),
                 ec.message());
        return Error::InvalidValue;
      }

      if (not create_directories(config_->base_path_, ec)) {
        if (ec) {
          SL_ERROR(logger_,
                   "Failed to create 'base_path' directory: {} ({}: {})",
                   config_->base_path_,
                   ec.value(),
                   ec.message());
          return Error::InvalidValue;
        }
      }
    } else if (ec) {
      SL_ERROR(logger_,
               "Failed to check existence of 'base_path': {} ({}: {})",
               config_->base_path_,
               ec.value(),
               ec.message());
      return Error::InvalidValue;
    }

    // Now it should exist; ensure it's a directory
    if (not is_directory(config_->base_path_, ec)) {
      if (ec) {
        SL_ERROR(logger_,
                 "Failed to check if 'base_path' is a directory: {} ({}: {})",
                 config_->base_path_,
                 ec.value(),
                 ec.message());
      } else {
        SL_ERROR(logger_,
                 "The 'base_path' exists but is not a directory: {}",
                 config_->base_path_);
      }
      return Error::InvalidValue;
    }


    // Helper to resolve general paths: if provided via CLI -> relative to CWD,
    // else if provided via config file -> relative to config file dir,
    // else fallback to CWD. Always normalize.
    auto resolve_relative = [&](const std::filesystem::path &path,
                                const char *cli_option_name) {
      if (path.is_absolute()) {
        return weakly_canonical(path);
      }
      // Was this option passed explicitly on CLI?
      if (find_argument<std::string>(cli_values_map_, cli_option_name)
              .has_value()) {
        return weakly_canonical(std::filesystem::current_path() / path);
      }
      // Otherwise, prefer resolving relative to config file location if present
      if (auto cfg = find_argument<std::string>(cli_values_map_, "config");
          cfg.has_value()) {
        return weakly_canonical(std::filesystem::path(*cfg).parent_path()
                                / path);
      }
      // Fallback: current working directory
      return weakly_canonical(std::filesystem::current_path() / path);
    };

    config_->modules_dir_ =
        resolve_relative(config_->modules_dir_, "modules-dir");
    if (not is_directory(config_->modules_dir_)) {
      SL_ERROR(logger_,
               "The 'modules_dir' does not exist or is not a directory: {}",
               config_->modules_dir_);
      return Error::InvalidValue;
    }

    if (not config_->bootnodes_file_.empty()) {
      config_->bootnodes_file_ =
          resolve_relative(config_->bootnodes_file_, "bootnodes");
      if (not is_regular_file(config_->bootnodes_file_)) {
        SL_ERROR(logger_,
                 "The 'bootnodes' file does not exist or is not a file: {}",
                 config_->bootnodes_file_);
        return Error::InvalidValue;
      }
    }

    if (not config_->validator_registry_path_.empty()) {
      config_->validator_registry_path_ = resolve_relative(
          config_->validator_registry_path_, "validator-registry-path");
      if (not is_regular_file(config_->validator_registry_path_)) {
        SL_ERROR(
            logger_,
            "The 'validator_registry_path' does not exist or is not a file: {}",
            config_->validator_registry_path_);
        return Error::InvalidValue;
      }
    }

    if (config_->genesis_config_path_.empty()) {
      SL_ERROR(logger_, "The 'genesis' path must be provided");
      return Error::InvalidValue;
    }

    config_->genesis_config_path_ =
        resolve_relative(config_->genesis_config_path_, "genesis");
    if (not is_regular_file(config_->genesis_config_path_)) {
      SL_ERROR(logger_,
               "The 'genesis' file does not exist or is not a file: {}",
               config_->genesis_config_path_);
      return Error::InvalidValue;
    }

    if constexpr (QLEAN_ENABLE_SHADOW) {
      if (auto value = find_argument<double>(
              cli_values_map_, "shadow-xmss-aggregate-signatures-rate")) {
        config_->fake_xmss_aggregate_signatures_rate_ = value.value();
      }
      if (auto value = find_argument<double>(
              cli_values_map_,
              "shadow-xmss-verify-aggregated-signatures-rate")) {
        config_->fake_xmss_verify_aggregated_signatures_rate_ = value.value();
      }
      config_->xmss_keypair_ = crypto::xmss::XmssProviderFake::loadKeypair(
          config_->xmss_secret_key_path_.string());
    } else {
      // Validate and load XMSS keys (mandatory)
      if (config_->xmss_public_key_path_.empty()) {
        SL_ERROR(logger_,
                 "The '--xmss-pk' (XMSS public key) path must be provided");
        return Error::InvalidValue;
      }
      if (config_->xmss_secret_key_path_.empty()) {
        SL_ERROR(logger_,
                 "The '--xmss-sk' (XMSS secret key) path must be provided");
        return Error::InvalidValue;
      }

      config_->xmss_public_key_path_ =
          resolve_relative(config_->xmss_public_key_path_, "xmss-pk");
      if (not is_regular_file(config_->xmss_public_key_path_)) {
        SL_ERROR(logger_,
                 "The 'xmss-pk' file does not exist or is not a file: {}",
                 config_->xmss_public_key_path_);
        return Error::InvalidValue;
      }

      config_->xmss_secret_key_path_ =
          resolve_relative(config_->xmss_secret_key_path_, "xmss-sk");
      if (not is_regular_file(config_->xmss_secret_key_path_)) {
        SL_ERROR(logger_,
                 "The 'xmss-sk' file does not exist or is not a file: {}",
                 config_->xmss_secret_key_path_);
        return Error::InvalidValue;
      }

      // Load XMSS keypair from JSON files
      OUTCOME_TRY(
          keypair,
          crypto::xmss::loadKeypairFromJson(config_->xmss_secret_key_path_,
                                            config_->xmss_public_key_path_));
      config_->xmss_keypair_ = std::move(keypair);
      SL_INFO(logger_, "Loaded XMSS keypair from:");
      SL_INFO(logger_, "  Public key: {}", config_->xmss_public_key_path_);
      SL_INFO(logger_, "  Secret key: {}", config_->xmss_secret_key_path_);
    }

    // Load validator keys manifest (mandatory)
    if (config_->validator_keys_manifest_path_.empty()) {
      SL_ERROR(logger_,
               "The '--validator-keys-manifest' path must be provided");
      return Error::InvalidValue;
    }

    config_->validator_keys_manifest_path_ = resolve_relative(
        config_->validator_keys_manifest_path_, "validator-keys-manifest");
    if (not is_regular_file(config_->validator_keys_manifest_path_)) {
      SL_ERROR(logger_,
               "The 'validator-keys-manifest' file does not exist or is not a "
               "file: {}",
               config_->validator_keys_manifest_path_);
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

    // Resolve database path against base_path_ when relative
    auto make_absolute = [&](const std::filesystem::path &path) {
      return weakly_canonical(
          path.is_absolute() ? path : (config_->base_path_ / path));
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
    if (find_argument(cli_values_map_, "metrics-host")) {
      find_argument<std::string>(
          cli_values_map_, "metrics-host", [&](const std::string &value) {
            boost::beast::error_code ec;
            auto address = boost::asio::ip::make_address(value, ec);
            if (!ec) {
              config_->metrics_.endpoint = {address,
                                            config_->metrics_.endpoint.port()};
              if (not config_->metrics_.enabled.has_value()) {
                config_->metrics_.enabled = true;
              }
            } else {
              std::cerr
                  << "Option --metrics-host has invalid value\n"
                  << "Try run with option '--help' for more information\n";
              fail = true;
            }
          });
    } else if (find_argument(cli_values_map_, "prometheus-host")) {
      std::cerr << "Option --prometheus-host is deprecated: "
                   "use --metric-host instead\n";
      find_argument<std::string>(
          cli_values_map_, "prometheus-host", [&](const std::string &value) {
            boost::beast::error_code ec;
            auto address = boost::asio::ip::make_address(value, ec);
            if (!ec) {
              config_->metrics_.endpoint = {address,
                                            config_->metrics_.endpoint.port()};
              if (not config_->metrics_.enabled.has_value()) {
                config_->metrics_.enabled = true;
              }
            } else {
              std::cerr
                  << "Option --prometheus-host has invalid value\n"
                  << "Try run with option '--help' for more information\n";
              fail = true;
            }
          });
    }
    if (fail) {
      return Error::CliArgsParseFailed;
    }

    fail = false;
    if (find_argument(cli_values_map_, "metrics-port")) {
      find_argument<uint16_t>(
          cli_values_map_, "metrics-port", [&](const uint16_t &value) {
            if (value > 0 and value <= 65535) {
              config_->metrics_.endpoint = {
                  config_->metrics_.endpoint.address(),
                  static_cast<uint16_t>(value)};
              if (not config_->metrics_.enabled.has_value()) {
                config_->metrics_.enabled = true;
              }
            } else {
              std::cerr
                  << "Option --metric-port has invalid value\n"
                  << "Try run with option '--help' for more information\n";
              fail = true;
            }
          });
    } else if (find_argument(cli_values_map_, "prometheus-port")) {
      std::cerr << "Option --prometheus-port is deprecated: "
                   "use --metric-port instead\n";
      find_argument<uint16_t>(
          cli_values_map_, "prometheus-port", [&](const uint16_t &value) {
            if (value > 0 and value <= 65535) {
              config_->metrics_.endpoint = {
                  config_->metrics_.endpoint.address(),
                  static_cast<uint16_t>(value)};
              if (not config_->metrics_.enabled.has_value()) {
                config_->metrics_.enabled = true;
              }
            } else {
              std::cerr
                  << "Option --prometheus-port has invalid value\n"
                  << "Try run with option '--help' for more information\n";
              fail = true;
            }
          });
    }
    if (fail) {
      return Error::CliArgsParseFailed;
    }

    if (find_argument(cli_values_map_, "metrics-disable")) {
      config_->metrics_.enabled = false;
    } else if (find_argument(cli_values_map_, "prometheus-disable")) {
      std::cerr << "Option --prometheus-disable is deprecated: "
                   "use --metric-disable instead\n";
      config_->metrics_.enabled = false;
    }
    if (not config_->metrics_.enabled.has_value()) {
      config_->metrics_.enabled = false;
    }

    if (not config_->node_key_.has_value()) {
      config_->node_key_ = randomKeyPair();
      SL_INFO(logger_, "Generating random node key");
    }

    return outcome::success();
  }

}  // namespace lean::app
