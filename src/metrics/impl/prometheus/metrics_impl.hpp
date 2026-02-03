/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "metrics/metrics.hpp"

namespace prometheus {
  class Counter;
  class Gauge;
  class Summary;
  class Histogram;
}  // namespace prometheus

namespace lean::metrics {

  class PrometheusCounter : public Counter {
    friend class PrometheusRegistry;
    prometheus::Counter &m_;

   public:
    PrometheusCounter(prometheus::Counter &m);

    [[nodiscard]] double value() const override;
    void inc() override;
    void inc(double val) override;
  };

  class PrometheusGauge : public Gauge {
    friend class PrometheusRegistry;
    prometheus::Gauge &m_;

   public:
    PrometheusGauge(prometheus::Gauge &m);

    [[nodiscard]] double value() const override;
    void inc() override;
    void inc(double val) override;
    void dec() override;
    void dec(double val) override;
    void set(double val) override;
  };

  class PrometheusSummary : public Summary {
    friend class PrometheusRegistry;
    prometheus::Summary &m_;

   public:
    PrometheusSummary(prometheus::Summary &m);

    void observe(double value) override;
  };

  class PrometheusHistogram : public Histogram {
    friend class PrometheusRegistry;
    prometheus::Histogram &m_;

   public:
    PrometheusHistogram(prometheus::Histogram &m);

    void observe(double value) override;
  };

}  // namespace lean::metrics
