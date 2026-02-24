/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <thread>

#include <qtils/shared_ref.hpp>

#include "log/logger.hpp"

namespace boost::asio {
  class io_context;
}  // namespace boost::asio

namespace lean {
  class ForkChoiceStore;
}  // namespace lean

namespace lean::metrics {
  class Handler;
}  // namespace lean::metrics

namespace lean::app {
  class Configuration;
  class StateManager;

  class HttpServer : public std::enable_shared_from_this<HttpServer> {
   public:
    HttpServer(qtils::SharedRef<log::LoggingSystem> logsys,
               qtils::SharedRef<StateManager> state_manager,
               qtils::SharedRef<Configuration> app_config,
               qtils::SharedRef<metrics::Handler> metrics_handler,
               qtils::SharedRef<ForkChoiceStore> fork_choice_store);
    ~HttpServer();

    void start();
    void stop();

   private:
    log::Logger log_;
    qtils::SharedRef<Configuration> app_config_;
    qtils::SharedRef<metrics::Handler> metrics_handler_;
    qtils::SharedRef<ForkChoiceStore> fork_choice_store_;
    std::shared_ptr<boost::asio::io_context> io_context_;
    std::optional<std::thread> io_thread_;
  };
}  // namespace lean::app
