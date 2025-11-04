/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include "metrics/metrics.hpp"

namespace lean::metrics {
  class Registry;

  /**
   * @brief Metrics implementation that holds all application metrics
   *
   * This class contains all metrics from all modules (application, fork choice,
   * state transition, etc.) Metrics are initialized once in the constructor and
   * accessed directly as member variables.
   */
  class MetricsImpl : public Metrics {
   public:
    explicit MetricsImpl(std::shared_ptr<Registry> registry);

   private:
    std::shared_ptr<Registry> registry_;

   public:
#define METRIC_GAUGE(field, name, help) \
 private:                               \
  Gauge *metric_##field##_;             \
                                        \
 public:                                \
  Gauge *field() override;
#define METRIC_GAUGE_LABELS(field, name, help, ...) \
 public:                                            \
  Gauge *field(const Labels &labels) override;
#define METRIC_COUNTER(field, name, help) \
 private:                                 \
  Counter *metric_##field##_;             \
                                          \
 public:                                  \
  Counter *field() override;
#define METRIC_COUNTER_LABELS(field, name, help, ...) \
 public:                                              \
  Counter *field(const Labels &labels) override;
#define METRIC_HISTOGRAM(field, name, help, ...) \
 private:                                        \
  Histogram *metric_##field##_;                  \
                                                 \
 public:                                         \
  Histogram *field() override;
#define METRIC_HISTOGRAM_LABELS(field, name, help, ...) \
 public:                                                \
  Histogram *field(const Labels &labels) override;

#include "metrics/all_metrics.def"

#undef METRIC_GAUGE
#undef METRIC_GAUGE_LABELS
#undef METRIC_COUNTER
#undef METRIC_COUNTER_LABELS
#undef METRIC_HISTOGRAM
#undef METRIC_HISTOGRAM_LABELS
  };

}  // namespace lean::metrics
