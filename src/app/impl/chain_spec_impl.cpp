/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app/impl/chain_spec_impl.hpp"

#include <charconv>
#include <sstream>
#include <system_error>

#include <app/configuration.hpp>
#include <libp2p/multi/multiaddress.hpp>
#include <libp2p/peer/peer_id.hpp>

#include "modules/networking/read_nodes_yaml.hpp"
#include "serde/enr.hpp"

OUTCOME_CPP_DEFINE_CATEGORY(lean::app, ChainSpecImpl::Error, e) {
  using E = lean::app::ChainSpecImpl::Error;
  switch (e) {
    case E::MISSING_ENTRY:
      return "A required entry is missing in the config file";
    case E::MISSING_PEER_ID:
      return "Peer id is missing in a multiaddress provided in the config file";
    case E::PARSER_ERROR:
      return "Internal parser error";
    case E::NOT_IMPLEMENTED:
      return "Known entry name, but parsing not implemented";
  }
  return "Unknown error in ChainSpecImpl";
}

namespace lean::app {

  ChainSpecImpl::ChainSpecImpl(qtils::SharedRef<log::LoggingSystem> logsys,
                               qtils::SharedRef<Configuration> app_config)
      : log_(logsys->getLogger("ChainSpec", "application")),
        app_config_(std::move(app_config)) {
    if (auto res = load(); res.has_error()) {
      SL_CRITICAL(log_, "Can't init chain spec: {}", res.error());
      qtils::raise(res.error());
    }
  }

  outcome::result<void> ChainSpecImpl::load() {
    OUTCOME_TRY(loadBootNodes());

    return outcome::success();
  }

  outcome::result<void> ChainSpecImpl::loadBootNodes() {
    // Check if bootnodes file is specified in configuration
    if (app_config_->bootnodesFile().empty()) {
      // No bootnodes file specified, return success with empty boot_nodes_
      SL_DEBUG(log_,
               "No bootnodes file specified, using empty boot nodes list");
      return outcome::success();
    }

    auto parsed_res = readNodesYaml(app_config_->bootnodesFile());
    if (parsed_res.has_error()) {
      SL_ERROR(log_,
               "Failed to parse bootnodes YAML file {}: {}",
               app_config_->bootnodesFile().string(),
               parsed_res.error().message());
      return Error::PARSER_ERROR;
    }

    auto parsed_nodes = std::move(parsed_res.value());

    for (const auto &invalid_enr : parsed_nodes.invalid_entries) {
      SL_WARN(log_, "Failed to decode ENR entry '{}'", invalid_enr);
    }

    std::vector<app::BootnodeInfo> bootnode_infos;
    bootnode_infos.reserve(parsed_nodes.parsed.size());

    for (const auto &entry : parsed_nodes.parsed) {
      try {
        auto peer_id = entry.enr.peerId();
        auto multiaddr = entry.enr.connectAddress();

        bootnode_infos.emplace_back(std::move(multiaddr), std::move(peer_id));
      } catch (const std::exception &e) {
        SL_WARN(log_,
                "Failed to extract peer info from ENR '{}': {}",
                entry.raw,
                e.what());
      }
    }

    bootnodes_ = app::Bootnodes(std::move(bootnode_infos));
    SL_INFO(log_,
            "Loaded {} boot nodes from {} ({} invalid entries skipped)",
            bootnodes_.size(),
            app_config_->bootnodesFile().string(),
            parsed_nodes.invalid_entries.size());

    return outcome::success();
  }

}  // namespace lean::app
