/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>

#include <boost/property_tree/json_parser.hpp>
#include <qtils/enum_error_code.hpp>

#include "app/chain_spec.hpp"
#include "log/logger.hpp"

namespace lean::app {
  class Configuration;
}
namespace lean::log {
  class LoggingSystem;
}

namespace lean::app {

  class ChainSpecImpl : public ChainSpec {
   public:
    enum class Error {
      MISSING_ENTRY = 1,
      MISSING_PEER_ID,
      PARSER_ERROR,
      NOT_IMPLEMENTED
    };

    ChainSpecImpl(qtils::SharedRef<log::LoggingSystem> logsys,
                  qtils::SharedRef<Configuration> app_config);

    const std::string &id() const override {
      return id_;
    }

    const std::vector<NodeAddress> &bootNodes() const override {
      return boot_nodes_;
    }

    const qtils::ByteVec &genesisHeader() const override {
      return genesis_header_;
    }

    const std::map<qtils::ByteVec, qtils::ByteVec> &genesisState()
        const override {
      return genesis_state_;
    }

   private:
    outcome::result<void> loadFromJson(const std::filesystem::path &file_path);
    outcome::result<void> loadFields(const boost::property_tree::ptree &tree);
    outcome::result<void> loadGenesis(const boost::property_tree::ptree &tree);
    outcome::result<void> loadBootNodes(
        const boost::property_tree::ptree &tree);

    template <typename T>
    outcome::result<std::decay_t<T>> ensure(std::string_view entry_name,
                                            boost::optional<T> opt_entry) {
      if (not opt_entry) {
        log_->error("Required '{}' entry not found in the chain spec",
                    entry_name);
        return Error::MISSING_ENTRY;
      }
      return opt_entry.value();
    }

    log::Logger log_;
    std::string id_;
    std::vector<NodeAddress> boot_nodes_;
    qtils::ByteVec genesis_header_;
    std::map<qtils::ByteVec, qtils::ByteVec> genesis_state_;
  };

}  // namespace lean::app

OUTCOME_HPP_DECLARE_ERROR(lean::app, ChainSpecImpl::Error)
