/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>
#include "metrics/metrics.hpp"
#include "metrics/registry.hpp"

namespace lean::metrics {

  struct GaugeHelper {
    // Constructor without labels
    GaugeHelper(metrics::Registry& registry,
                const std::string &name,
                const std::string &help)
        : registry_(&registry), name_(name) {
      registry.registerGaugeFamily(name, help);
      metric_ = registry.registerGaugeMetric(name);
    }

    // Constructor with labels (all labels must be std::string or convertible to std::string)
    template<typename... Labels,
             typename = std::enable_if_t<(std::is_convertible_v<Labels, std::string> && ...)>>
    GaugeHelper(metrics::Registry& registry,
                const std::string &name,
                const std::string &help,
                Labels&&... labels)
        : registry_(&registry), name_(name) {
      static_assert(sizeof...(Labels) > 0, "Use this constructor only for metrics with labels");
      registry.registerGaugeFamily(name, help);
      metric_ = nullptr;  // Will be created via operator()
    }

    auto *operator->() const {
      return metric_;
    }

    /**
     * @brief Create a gauge instance with specific label values
     * Usage: gauge_helper({{"label1", value1}, {"label2", value2}})
     */
    Gauge* operator()(const std::map<std::string, std::string>& labels) {
      return registry_->registerGaugeMetric(name_, labels);
    }

    metrics::Registry* registry_;
    std::string name_;
    Gauge *metric_;
  };

}  // namespace lean::metrics