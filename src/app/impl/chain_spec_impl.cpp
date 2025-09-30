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
#include <yaml-cpp/yaml.h>

#include "serde/enr.hpp"
#include "types/block_header.hpp"

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

  namespace pt = boost::property_tree;

  ChainSpecImpl::ChainSpecImpl(qtils::SharedRef<log::LoggingSystem> logsys,
                               qtils::SharedRef<Configuration> app_config)
      : log_(logsys->getLogger("ChainSpec", "application")),
        app_config_(std::move(app_config)) {
    if (auto res = loadFromJson(app_config_->specFile()); res.has_error()) {
      SL_CRITICAL(log_, "Can't init chain spec by json-file: {}", res.error());
      qtils::raise(res.error());
    }
  }

  outcome::result<void> ChainSpecImpl::loadFromJson(
      const std::filesystem::path &file_path) {
    pt::ptree tree;
    try {
      pt::read_json(file_path, tree);
    } catch (pt::json_parser_error &e) {
      log_->error(
          "Parser error: {}, line {}: {}", e.filename(), e.line(), e.message());
      return Error::PARSER_ERROR;
    }

    OUTCOME_TRY(loadFields(tree));
    OUTCOME_TRY(loadGenesis(tree));
    OUTCOME_TRY(loadBootNodes(tree));

    return outcome::success();
  }

  outcome::result<void> ChainSpecImpl::loadFields(
      const boost::property_tree::ptree &tree) {
    OUTCOME_TRY(id, ensure("id", tree.get_child_optional("id")));
    id_ = id.get<std::string>("");

    return outcome::success();
  }

  outcome::result<void> ChainSpecImpl::loadGenesis(
      const boost::property_tree::ptree &tree) {
    // OUTCOME_TRY(
    //     genesis_header_hex,
    //     ensure("genesis_header", tree.get_child_optional("genesis_header")));
    // OUTCOME_TRY(genesis_header_encoded,
    //             qtils::ByteVec::fromHex(genesis_header_hex.data()));

    BlockHeader header;
    header.proposer_index = -1ull;
    OUTCOME_TRY(genesis_header_encoded, encode(header));

    genesis_header_ = std::move(genesis_header_encoded);
    return outcome::success();
  }

  outcome::result<void> ChainSpecImpl::loadBootNodes(
      const boost::property_tree::ptree &tree) {
    // Check if bootnodes file is specified in configuration
    if (app_config_->bootnodesFile().empty()) {
      // No bootnodes file specified, return success with empty boot_nodes_
      SL_DEBUG(log_, "No bootnodes file specified, using empty boot nodes list");
      return outcome::success();
    }

    try {
      YAML::Node nodes = YAML::LoadFile(app_config_->bootnodesFile());

      if (!nodes.IsSequence()) {
        SL_ERROR(log_, "Bootnodes YAML file must contain a sequence");
        return Error::PARSER_ERROR;
      }

      std::vector<app::BootnodeInfo> bootnode_infos;
      
      for (const auto& node : nodes) {
        if (!node.IsScalar()) {
          SL_ERROR(log_, "Each bootnode entry must be a string (ENR)");
          return Error::PARSER_ERROR;
        }

        std::string enr_string = node.as<std::string>();
        if (enr_string.empty()) {
          SL_WARN(log_, "Skipping empty ENR entry");
          continue;
        }

        // Parse ENR to extract multiaddress and peer ID
        auto enr_result = lean::enr::decode(enr_string);
        if (!enr_result.has_value()) {
          SL_WARN(log_, "Failed to parse ENR {}: {}", enr_string, enr_result.error());
          continue;
        }

        const auto& enr = enr_result.value();
        try {
          auto peer_id = enr.peerId();
          auto multiaddr = enr.connectAddress();
          
          bootnode_infos.emplace_back(std::move(multiaddr), std::move(peer_id));
          SL_DEBUG(log_, "Added boot node: {} -> peer={}, address={}",
                      enr_string, 
                      peer_id, 
                      multiaddr.getStringAddress());
        } catch (const std::exception& e) {
          SL_WARN(log_, "Failed to extract peer info from ENR {}: {}", enr_string, e.what());
        }
      }

      bootnodes_ = app::Bootnodes(std::move(bootnode_infos));
      SL_INFO(log_, "Loaded {} boot nodes from {}", bootnodes_.size(), app_config_->bootnodesFile().string());

    } catch (const YAML::Exception& e) {
      SL_ERROR(log_, "Failed to parse bootnodes YAML file {}: {}",
                  app_config_->bootnodesFile().string(), e.what());
      return Error::PARSER_ERROR;
    } catch (const std::exception& e) {
      SL_ERROR(log_, "Failed to load bootnodes file {}: {}",
                  app_config_->bootnodesFile().string(), e.what());
      return Error::PARSER_ERROR;
    }

    return outcome::success();
  }

}  // namespace lean::app