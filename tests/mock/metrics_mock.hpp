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
    void inc() override {}
    void inc(double val) override {}
    void dec() override {}
    void dec(double val) override {}
    void set(double val) override {}
  };

  class CounterMock : public Counter {
   public:
    void inc() override {}
    void inc(double val) override {}
  };

  class HistogramMock : public Histogram {
   public:
    void observe(const double value) override {}
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
#define METRIC_COUNTER(field, name, help) \
  Counter *field() override {             \
    return &counter_;                     \
  }
#define METRIC_COUNTER_LABELS(field, name, help, ...) \
  Counter *field(const Labels &labels) override {     \
    return &counter_;                                 \
  }
#define METRIC_HISTOGRAM(field, name, help, ...) \
  Histogram *field() override {                  \
    return &histogram_;                          \
  }
#define METRIC_HISTOGRAM_LABELS(field, name, help, ...) \
  Histogram *field(const Labels &labels) override {     \
    return &histogram_;                                 \
  }

#include "metrics/all_metrics.def"

#undef METRIC_GAUGE
#undef METRIC_GAUGE_LABELS
#undef METRIC_COUNTER
#undef METRIC_COUNTER_LABELS
#undef METRIC_HISTOGRAM
#undef METRIC_HISTOGRAM_LABELS

   private:
    GaugeMock gauge_;
    CounterMock counter_;
    HistogramMock histogram_;
  };
}  // namespace lean::metrics
