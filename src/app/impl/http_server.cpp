/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app/impl/http_server.hpp"

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <qtils/bytestr.hpp>

#include "app/configuration.hpp"
#include "app/state_manager.hpp"
#include "blockchain/fork_choice.hpp"
#include "metrics/handler.hpp"
#include "utils/http.hpp"

namespace lean::app {
  HttpServer::HttpServer(qtils::SharedRef<log::LoggingSystem> logsys,
                         qtils::SharedRef<StateManager> state_manager,
                         qtils::SharedRef<Configuration> app_config,
                         qtils::SharedRef<metrics::Handler> metrics_handler,
                         qtils::SharedRef<ForkChoiceStore> fork_choice_store)
      : log_{logsys->getLogger("HttpServer", "http")},
        app_config_{std::move(app_config)},
        metrics_handler_{std::move(metrics_handler)},
        fork_choice_store_{std::move(fork_choice_store)} {
    state_manager->takeControl(*this);
  }

  HttpServer::~HttpServer() {
    stop();
  }

  void HttpServer::start() {
    io_context_ = std::make_shared<boost::asio::io_context>();
    http::ServerConfig config{
        .endpoint = app_config_->metrics().endpoint,
        .on_request =
            [weak_self{weak_from_this()}](http::Request request) {
              http::Response response;
              auto self = weak_self.lock();
              if (not self) {
                response.result(boost::beast::http::status::bad_gateway);
                return response;
              }
              std::string_view url{request.target()};
              SL_INFO(self->log_,
                      "{} {}",
                      std::string_view{request.method_string()},
                      url);
              if (url == "/metrics") {
                response.set(boost::beast::http::field::content_type,
                             "text/plain; charset=utf-8");
                response.body() = self->metrics_handler_->collect();
                return response;
              }
              if (url == "/lean/v0/health") {
                response.set(boost::beast::http::field::content_type,
                             "application/json");
                response.body() =
                    R"({"status":"healthy","service":"qlean-api"})";
                return response;
              }
              if (url == "/lean/v0/states/finalized") {
                auto finalized = self->fork_choice_store_->getLatestFinalized();
                if (auto state_res =
                        self->fork_choice_store_->getState(finalized.root)) {
                  auto &state = state_res.value();
                  response.set(boost::beast::http::field::content_type,
                               "application/octet-stream");
                  response.body() = qtils::byte2str(encode(*state).value());
                } else {
                  response.result(
                      boost::beast::http::status::internal_server_error);
                }
                return response;
              }
              if (url == "/lean/v0/checkpoints/justified") {
                auto justified = self->fork_choice_store_->getLatestJustified();
                response.set(boost::beast::http::field::content_type,
                             "application/json");
                response.body() = std::format(R"({{"root":"0x{}","slot":{}}})",
                                              justified.root.toHex(),
                                              justified.slot);
                return response;
              }
              response.result(boost::beast::http::status::not_found);
              return response;
            },
    };
    auto listen_res = http::serve(log_, *io_context_, config);
    if (not listen_res.has_value()) {
      SL_WARN(log_, "listen error: {}", listen_res.error());
    }
    io_thread_.emplace([io_context{io_context_}] {
      auto work_guard = boost::asio::make_work_guard(*io_context);
      io_context->run();
    });
  }

  void HttpServer::stop() {
    if (io_thread_.has_value()) {
      io_context_->stop();
      io_thread_->join();
    }
  }
}  // namespace lean::app
