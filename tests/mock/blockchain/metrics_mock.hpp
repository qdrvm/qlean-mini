/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "metrics/metrics.hpp"

namespace lean::metrics {
  class GaugeMock : public Gauge {
   public:
    // Gauge
    void inc() override {}
    void inc(double val) override {}
    void dec() override {}
    void dec(double val) override {}
    void set(double val) override {}
  };

  class MetricsMock : public Metrics {
   public:
#define METRIC_GAUGE(field, name, help) \
  Gauge *field() override {             \
    return &gauge_;                     \
  }
#define METRIC_GAUGE_LABELS(field, name, help, ...) \
  Gauge *field(const Labels &labels) override {     \
    return &gauge_;                                 \
  }

#include "metrics/all_metrics.def"

#undef METRIC_GAUGE
#undef METRIC_GAUGE_LABELS

   private:
    GaugeMock gauge_;
  };
}  // namespace lean::metrics
