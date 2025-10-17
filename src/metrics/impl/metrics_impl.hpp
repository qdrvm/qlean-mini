/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>

#include "metrics/metrics.hpp"
#include "metrics/helpers.hpp"

namespace lean::metrics {

  /**
   * @brief Metrics implementation that holds all application metrics
   *
   * This class contains all metrics from all modules (application, fork choice, state transition, etc.)
   * Metrics are initialized once in the constructor and accessed directly as member variables.
   */
  class MetricsImpl : public Metrics {
   public:
    explicit MetricsImpl(std::shared_ptr<Registry> registry);

    /**
     * @brief Factory method to create MetricsImpl instance
     */
    static std::shared_ptr<MetricsImpl> create();

    /**
     * @brief Get the underlying registry
     */
    Registry& getRegistry() override {
      return *registry_;
    }

   private:
    std::shared_ptr<Registry> registry_;

   public:
    // Define all metric members using X-macro pattern
    // Format:
    //   METRIC_GAUGE(field_name,
    //                "prometheus_metric_name",
    //                "help_text")
    //
    //   METRIC_GAUGE(field_name,
    //                "prometheus_metric_name",
    //                "help_text",
    //                "label1",
    //                "label2",
    //                ...)
    //
    // Parameters:
    //   - field_name: Required - C++ member variable name
    //   - prometheus_metric_name: Required - Prometheus metric name
    //   - help_text: Required - Metric description
    //   - labels: Optional - Any number of label names
    #define METRIC_GAUGE(name, ...) GaugeHelper name;

    // Include all metric definitions
    #include "../../app/application_metrics.def"
    #include "../../blockchain/fork_choice_metrics.def"

    #undef METRIC_GAUGE

  };

}  // namespace lean::metrics