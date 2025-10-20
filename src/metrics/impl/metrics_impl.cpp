/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "metrics/impl/metrics_impl.hpp"

#include "metrics/registry.hpp"

namespace lean::metrics {

  MetricsImpl::MetricsImpl(std::shared_ptr<Registry> registry)
      : registry_{std::move(registry)} {
#define METRIC_GAUGE(field, name, help)       \
  registry_->registerGaugeFamily(name, help); \
  metric_##field##_ = registry_->registerGaugeMetric(name);
#define METRIC_GAUGE_LABELS(field, name, help, ...) \
  registry_->registerGaugeFamily(name, help);

#include "metrics/all_metrics.def"

#undef METRIC_GAUGE
#undef METRIC_GAUGE_LABELS
  }

#define METRIC_GAUGE(field, name, help) \
  Gauge *MetricsImpl::field() {         \
    return metric_##field##_;           \
  }
#define METRIC_GAUGE_LABELS(field, name, help, ...)      \
  Gauge *MetricsImpl::field(const Labels &labels) {      \
    return registry_->registerGaugeMetric(name, labels); \
  }

#include "metrics/all_metrics.def"

#undef METRIC_GAUGE
#undef METRIC_GAUGE_LABELS
}  // namespace lean::metrics
