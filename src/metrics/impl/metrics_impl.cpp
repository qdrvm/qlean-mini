/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "metrics/impl/metrics_impl.hpp"
#include "metrics/impl/prometheus/registry_impl.hpp"
#include "metrics/metrics.hpp"

namespace lean::metrics {

  MetricsImpl::MetricsImpl(std::shared_ptr<Registry> registry)
      : registry_(std::move(registry))
        // Initialize all metrics from .def files
        #define METRIC_GAUGE(name, ...) , name(*registry_, __VA_ARGS__)

        #include "../../app/application_metrics.def"
        #include "../../blockchain/fork_choice_metrics.def"

        #undef METRIC_GAUGE
  {}

  std::shared_ptr<MetricsImpl> MetricsImpl::create() {
    return std::make_shared<MetricsImpl>(
        std::shared_ptr<Registry>(PrometheusRegistry::create()));
  }

}  // namespace lean::metrics
