/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app/impl/application_impl.hpp"

#include <thread>
#include <unistd.h>

#include "app/configuration.hpp"
#include "app/impl/watchdog.hpp"
#include "app/state_manager.hpp"
#include "app/timeline.hpp"
#include "clock/clock.hpp"
#include "log/logger.hpp"
#include "metrics/handler.hpp"
#include "metrics/impl/metrics_impl.hpp"
#include "metrics/metrics.hpp"
#include "se/impl/subscription_manager.hpp"

namespace lean::app {

  SeHolder::SeHolder(SePtr se) : se_(std::move(se)) {}

  SeHolder::~SeHolder() {
    se_->dispose();
  }

  ApplicationImpl::ApplicationImpl(
      qtils::SharedRef<log::LoggingSystem> logsys,
      qtils::SharedRef<Configuration> config,
      qtils::SharedRef<StateManager> state_manager,
      qtils::SharedRef<Watchdog> watchdog,
      qtils::SharedRef<HttpServer> http_server,
      qtils::SharedRef<metrics::Metrics> metrics,
      qtils::SharedRef<metrics::Handler> metrics_handler,
      qtils::SharedRef<clock::SystemClock> system_clock,
      qtils::SharedRef<Timeline> timeline,
      qtils::SharedRef<metrics::Registry> metrics_registry,
      std::shared_ptr<SeHolder>)
      : logger_(logsys->getLogger("Application", "application")),
        app_config_(std::move(config)),
        state_manager_(std::move(state_manager)),
        watchdog_(std::move(watchdog)),
        http_server_{std::move(http_server)},
        metrics_(std::move(metrics)),
        metrics_handler_(std::move(metrics_handler)),
        system_clock_(std::move(system_clock)),
        timeline_(std::move(timeline)) {
    metrics_handler_->registerCollectable(*metrics_registry);

    // Metric for exposing name and version of node
    metrics_
        ->app_build_info({
            {"name", app_config_->nodeName()},
            {"version", app_config_->nodeVersion()},
        })
        ->set(1);
  }

  void ApplicationImpl::run() {
    logger_->info("Start as node version '{}' named as '{}' with PID {}",
                  app_config_->nodeVersion(),
                  app_config_->nodeName(),
                  getpid());

    std::thread watchdog_thread([this] {
      soralog::util::setThreadName("watchdog");
      watchdog_->checkLoop(kWatchdogDefaultTimeout);
    });

    state_manager_->atShutdown([this] { watchdog_->stop(); });

    // Set process start time metric
    metrics_->app_process_start_time()->set(system_clock_->nowSec());

    state_manager_->run();

    watchdog_->stop();

    watchdog_thread.join();
  }

}  // namespace lean::app
