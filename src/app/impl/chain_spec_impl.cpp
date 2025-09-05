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
      : log_(logsys->getLogger("ChainSpec", "application")) {
    if (auto res = loadFromJson(app_config->specFile()); res.has_error()) {
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
    // TODO Not implemented
    return outcome::success();
  }

}  // namespace lean::app
