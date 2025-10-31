/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "metrics/impl/metrics_impl.hpp"

#include "metrics/registry.hpp"

namespace lean::metrics {

#define UNWRAP(...) __VA_ARGS__

  MetricsImpl::MetricsImpl(std::shared_ptr<Registry> registry)
      : registry_{std::move(registry)} {
#define METRIC_GAUGE(field, name, help)       \
  registry_->registerGaugeFamily(name, help); \
  metric_##field##_ = registry_->registerGaugeMetric(name);
#define METRIC_GAUGE_LABELS(field, name, help, label_names) \
  registry_->registerGaugeFamily(name, help);
#define METRIC_COUNTER(field, name, help)       \
  registry_->registerCounterFamily(name, help); \
  metric_##field##_ = registry_->registerCounterMetric(name);
#define METRIC_COUNTER_LABELS(field, name, help, label_names) \
  registry_->registerCounterFamily(name, help);
#define METRIC_HISTOGRAM(field, name, help, buckets) \
  registry_->registerHistogramFamily(name, help);    \
  metric_##field##_ =                                \
      registry_->registerHistogramMetric(name, {UNWRAP buckets});
#define METRIC_HISTOGRAM_LABELS(field, name, help, buckets, label_names) \
  registry_->registerHistogramFamily(name, help);

#include "metrics/all_metrics.def"

#undef METRIC_GAUGE
#undef METRIC_GAUGE_LABELS
#undef METRIC_COUNTER
#undef METRIC_COUNTER_LABELS
#undef METRIC_HISTOGRAM
#undef METRIC_HISTOGRAM_LABELS
  }

#define METRIC_GAUGE(field, name, help) \
  Gauge *MetricsImpl::field() {         \
    return metric_##field##_;           \
  }
#define METRIC_GAUGE_LABELS(field, name, help, label_names) \
  Gauge *MetricsImpl::field(const Labels &labels) {         \
    return registry_->registerGaugeMetric(name, labels);    \
  }
#define METRIC_COUNTER(field, name, help) \
  Counter *MetricsImpl::field() {         \
    return metric_##field##_;             \
  }
#define METRIC_COUNTER_LABELS(field, name, help, label_names) \
  Counter *MetricsImpl::field(const Labels &labels) {         \
    return registry_->registerCounterMetric(name, labels);    \
  }
#define METRIC_HISTOGRAM(field, name, help, buckets) \
  Histogram *MetricsImpl::field() {                  \
    return metric_##field##_;                        \
  }
#define METRIC_HISTOGRAM_LABELS(field, name, help, buckets, label_names)   \
  Histogram *MetricsImpl::field(const Labels &labels) {                    \
    static const std::vector<double> bucket_boundaries = {UNWRAP buckets}; \
    return registry_->registerHistogramMetric(                             \
        name, bucket_boundaries, labels);                                  \
  }

#include "metrics/all_metrics.def"

#undef METRIC_GAUGE
#undef METRIC_GAUGE_LABELS
#undef METRIC_COUNTER
#undef METRIC_COUNTER_LABELS
#undef METRIC_HISTOGRAM
#undef METRIC_HISTOGRAM_LABELS
}  // namespace lean::metrics
