/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "metrics/impl/prometheus/handler_impl.hpp"

#include <prometheus/text_serializer.h>

#include "log/logger.hpp"
#include "registry_impl.hpp"
#include "utils/retain_if.hpp"
// #include "utils/wptr.hpp"

using prometheus::Collectable;
using prometheus::MetricFamily;
using prometheus::TextSerializer;

std::vector<MetricFamily> CollectMetrics(
    const std::vector<std::weak_ptr<Collectable>> &collectables) {
  auto collected_metrics = std::vector<MetricFamily>{};

  for (auto &&wcollectable : collectables) {
    auto collectable = wcollectable.lock();
    if (!collectable) {
      continue;
    }

    auto &&metrics = collectable->Collect();
    collected_metrics.insert(collected_metrics.end(),
                             std::make_move_iterator(metrics.begin()),
                             std::make_move_iterator(metrics.end()));
  }

  return collected_metrics;
}

namespace lean::metrics {

  PrometheusHandler::PrometheusHandler(
      std::shared_ptr<log::LoggingSystem> logsys)
      : logger_{logsys->getLogger("PrometheusHandler", "metrics")} {}

  std::string PrometheusHandler::collect() {
    std::vector<MetricFamily> metrics;

    {
      std::lock_guard<std::mutex> lock{collectables_mutex_};
      metrics = CollectMetrics(collectables_);
    }

    const TextSerializer serializer;

    return serializer.Serialize(metrics);
  }

  // it is called once on init
  void PrometheusHandler::registerCollectable(Registry &registry) {
    auto *pregistry = dynamic_cast<PrometheusRegistry *>(&registry);
    if (pregistry) {
      registerCollectable(pregistry->registry());
    }
  }

  void PrometheusHandler::registerCollectable(
      const std::weak_ptr<Collectable> &collectable) {
    std::lock_guard<std::mutex> lock{collectables_mutex_};
    cleanupStalePointers(collectables_);
    collectables_.push_back(collectable);
  }

  void PrometheusHandler::removeCollectable(
      const std::weak_ptr<Collectable> &collectable) {
    std::lock_guard<std::mutex> lock{collectables_mutex_};
    retain_if(collectables_, [&](const std::weak_ptr<Collectable> &candidate) {
      // Check if wptrs references to the same data
      return not candidate.owner_before(collectable)
         and not collectable.owner_before(candidate);
    });
  }

  void PrometheusHandler::cleanupStalePointers(
      std::vector<std::weak_ptr<Collectable>> &collectables) {
    retain_if(collectables, [](const std::weak_ptr<Collectable> &candidate) {
      return not candidate.expired();
    });
  }

}  // namespace lean::metrics
