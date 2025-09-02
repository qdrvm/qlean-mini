/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include <prometheus/collectable.h>

#include "log/logger.hpp"
#include "metrics/handler.hpp"

namespace lean::metrics {

  class PrometheusHandler : public Handler {
   public:
    explicit PrometheusHandler(std::shared_ptr<log::LoggingSystem> logsys);
    ~PrometheusHandler() override = default;

    void registerCollectable(Registry &registry) override;

    void onSessionRequest(Session::Request request,
                          std::shared_ptr<Session> session) override;

   private:
    void registerCollectable(
        const std::weak_ptr<prometheus::Collectable> &collectable);
    void removeCollectable(
        const std::weak_ptr<prometheus::Collectable> &collectable);
    static void cleanupStalePointers(
        std::vector<std::weak_ptr<prometheus::Collectable>> &collectables);
    std::size_t writeResponse(std::shared_ptr<Session> session,
                              const Session::Request &request,
                              const std::string &body);

    std::shared_ptr<soralog::Logger> logger_;
    std::mutex collectables_mutex_;
    std::vector<std::weak_ptr<prometheus::Collectable>> collectables_;
  };

}  // namespace lean::metrics
