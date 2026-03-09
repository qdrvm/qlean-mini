/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <string>

namespace lean::metrics {

  class Registry;

  /**
   * @brief an interface to add request handler for metrics::Exposer
   * implementation generally will contain metrics serializer
   */
  class Handler {
   public:
    virtual ~Handler() = default;
    /**
     * @brief registers general type metrics registry for metrics collection
     */
    virtual void registerCollectable(Registry &registry) = 0;

    virtual std::string collect() = 0;
  };

}  // namespace lean::metrics
