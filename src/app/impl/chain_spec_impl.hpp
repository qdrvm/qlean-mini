/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>

#include <qtils/enum_error_code.hpp>

#include "app/bootnodes.hpp"
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

    const app::Bootnodes &getBootnodes() const override {
      return bootnodes_;
    }

   private:
    outcome::result<void> load();
    outcome::result<void> loadBootNodes();

    log::Logger log_;
    qtils::SharedRef<Configuration> app_config_;
    app::Bootnodes bootnodes_;
  };

}  // namespace lean::app

OUTCOME_HPP_DECLARE_ERROR(lean::app, ChainSpecImpl::Error)
