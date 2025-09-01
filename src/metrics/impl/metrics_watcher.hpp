/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <thread>
#include <filesystem>

#include "app/configuration.hpp"
#include "app/state_manager.hpp"
#include "metrics/metrics.hpp"
#include "qtils/outcome.hpp"

namespace lean::metrics {
  class Registry;
}

namespace lean::metrics {

  class MetricsWatcher final {
   public:
    MetricsWatcher(std::shared_ptr<app::StateManager> state_manager,
                   std::shared_ptr<app::Configuration> config);

    bool start();
    void stop();

   private:
    outcome::result<uintmax_t> measure_storage_size();

    std::shared_ptr<app::StateManager> state_manager_;
    std::shared_ptr<app::Configuration> app_config_;

    std::filesystem::path storage_path_;
    volatile bool shutdown_requested_ = false;
    std::thread thread_;

    // Metrics
    std::unique_ptr<metrics::Registry> metrics_registry_;
    metrics::Gauge *metric_storage_size_;
  };

}  // namespace lean::metrics
