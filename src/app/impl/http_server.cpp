/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app/impl/http_server.hpp"

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <qtils/bytestr.hpp>
#include <qtils/option_take.hpp>

#include "app/chain_spec.hpp"
#include "app/configuration.hpp"
#include "app/state_manager.hpp"
#include "blockchain/block_tree.hpp"
#include "blockchain/fork_choice_mutex.hpp"
#include "metrics/handler.hpp"
#include "serde/json.hpp"
#include "serde/serialization.hpp"
#include "types/fork_choice_api_json.hpp"
#include "types/state.hpp"
#include "utils/http.hpp"

namespace lean::app {
  struct Enabled {
    bool enabled;

    JSON_FIELDS(enabled);
  };

  HttpServer::HttpServer(
      qtils::SharedRef<log::LoggingSystem> logsys,
      qtils::SharedRef<StateManager> state_manager,
      qtils::SharedRef<Configuration> app_config,
      qtils::SharedRef<metrics::Handler> metrics_handler,
      qtils::SharedRef<app::ChainSpec> chain_spec,
      qtils::SharedRef<blockchain::BlockTree> block_tree,
      qtils::SharedRef<ForkChoiceStoreMutex> fork_choice_store)
      : log_{logsys->getLogger("HttpServer", "http")},
        app_config_{std::move(app_config)},
        metrics_handler_{std::move(metrics_handler)},
        chain_spec_{std::move(chain_spec)},
        block_tree_{std::move(block_tree)},
        fork_choice_store_{std::move(fork_choice_store)} {
    state_manager->takeControl(*this);
  }

  HttpServer::~HttpServer() {
    stop();
  }

  void HttpServer::start() {
    io_context_ = std::make_shared<boost::asio::io_context>();
    http::ServerConfig config_metrics{
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
              response.result(boost::beast::http::status::not_found);
              return response;
            },
    };
    http::ServerConfig config_api{
        .endpoint = app_config_->apiEndpoint(),
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
              if (url == "/lean/v0/health") {
                return http::respondJson(
                    R"({"status":"healthy","service":"lean-rpc-api"})");
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
                return http::respondJson(
                    std::format(R"({{"root":"0x{}","slot":{}}})",
                                justified.root.toHex(),
                                justified.slot));
              }
              if (url == "/lean/v0/fork_choice") {
                if (auto result_res =
                        self->fork_choice_store_->apiForkChoice()) {
                  auto &result = result_res.value();
                  return http::respondJson(
                      json::encode(json::NameCase::SNAKE, result));
                }
                response.result(
                    boost::beast::http::status::internal_server_error);
                return response;
              }
              if (url == "/lean/v0/admin/aggregator") {
                if (request.method() == boost::beast::http::verb::get) {
                  auto is_aggregator = self->chain_spec_->isAggregator();
                  return http::respondJson(
                      std::format(R"({{"is_aggregator":{}}})", is_aggregator));
                }
                if (request.method() == boost::beast::http::verb::post) {
                  Enabled body;
                  try {
                    json::decode(json::NameCase::SNAKE, body, request.body());
                  } catch (std::exception &e) {
                    response.result(boost::beast::http::status::bad_request);
                    response.body() = e.what();
                    return response;
                  }
                  auto enabled = body.enabled;
                  auto previous = self->chain_spec_->setIsAggregator(enabled);
                  return http::respondJson(
                      std::format(R"({{"is_aggregator":{},"previous":{}}})",
                                  enabled,
                                  previous));
                }
              }
              if (url == "/lean/v0/blocks/finalized") {
                auto finalized = self->fork_choice_store_->getLatestFinalized();
                auto block_result =
                    self->block_tree_->tryGetSignedBlock(finalized.root);
                if (not block_result.has_value()) {
                  response.result(
                      boost::beast::http::status::internal_server_error);
                  return response;
                }
                auto &block = block_result.value();
                if (not block.has_value()) {
                  response.result(boost::beast::http::status::not_found);
                  return response;
                }
                response.set(boost::beast::http::field::content_type,
                             "application/octet-stream");
                response.body() = qtils::byte2str(encode(*block).value());
                return response;
              }
              response.result(boost::beast::http::status::not_found);
              return response;
            },
    };
    if (app_config_->metrics().enabled.value_or(false)) {
      auto listen_res = http::serve(log_, *io_context_, config_metrics);
      if (not listen_res.has_value()) {
        SL_WARN(log_,
                "listen metrics {}:{} error: {}",
                config_metrics.endpoint.address().to_string(),
                config_metrics.endpoint.port(),
                listen_res.error());
      }
    }
    auto listen_res = http::serve(log_, *io_context_, config_api);
    if (not listen_res.has_value()) {
      SL_WARN(log_,
              "listen api {}:{} error: {}",
              config_api.endpoint.address().to_string(),
              config_api.endpoint.port(),
              listen_res.error());
    }
    io_thread_.emplace([io_context{io_context_}] {
      auto work_guard = boost::asio::make_work_guard(*io_context);
      io_context->run();
    });
  }

  void HttpServer::stop() {
    if (auto io_thread = qtils::optionTake(io_thread_)) {
      io_context_->stop();
      io_thread->join();
    }
  }
}  // namespace lean::app
