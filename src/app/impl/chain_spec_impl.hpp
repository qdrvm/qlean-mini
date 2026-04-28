/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <atomic>
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

namespace lean::metrics {
  class Metrics;
}  // namespace lean::metrics

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
                  qtils::SharedRef<metrics::Metrics> metrics,
                  qtils::SharedRef<Configuration> app_config);

    // ChainSpec
    const app::Bootnodes &getBootnodes() const override;
    bool isAggregator() const override;
    bool setIsAggregator(bool is_aggregator) override;

   private:
    outcome::result<void> load();
    outcome::result<void> loadBootNodes();
    void updateMetricIsAggregator();

    log::Logger log_;
    qtils::SharedRef<metrics::Metrics> metrics_;
    qtils::SharedRef<Configuration> app_config_;
    app::Bootnodes bootnodes_;
    std::atomic_bool is_aggregator_ = false;
  };

}  // namespace lean::app

OUTCOME_HPP_DECLARE_ERROR(lean::app, ChainSpecImpl::Error)
