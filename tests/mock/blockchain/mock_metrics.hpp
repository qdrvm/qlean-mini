/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/shared_ref.hpp>
#include "metrics/impl/metrics_impl.hpp"
#include "metrics/impl/prometheus/registry_impl.hpp"

namespace lean::metrics {

  /**
   * @brief Create a mock MetricsImpl for testing
   * @return SharedRef to MetricsImpl with a test registry
   */
  inline qtils::SharedRef<MetricsImpl> createMockMetrics() {
    return MetricsImpl::create();
  }

}  // namespace lean::metrics
