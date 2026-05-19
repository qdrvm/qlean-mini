/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include "blockchain/fork_choice.hpp"
#include "crypto/xmss/xmss_provider_impl.hpp"
#include "log/logger.hpp"
#include "mock/app/validator_keys_manifest_mock.hpp"
#include "mock/blockchain/block_storage_mock.hpp"
#include "mock/blockchain/block_tree_mock.hpp"
#include "mock/blockchain/validator_registry_mock.hpp"
#include "mock/metrics_mock.hpp"
#include "serde/json.hpp"
#include "types/signed_block.hpp"
#include "types/state.hpp"
#include "utils/http.hpp"

struct VerifySignaturesRequest {
  lean::State anchor_state;
  lean::SignedBlock signed_block;

  JSON_FIELDS(anchor_state, signed_block);
};

struct VerifySignaturesResponse {
  bool succeeded;
  std::optional<std::string> error;

  JSON_FIELDS(succeeded, error);
};

struct StateTransitionRequest {
  lean::State pre;
  std::vector<lean::Block> blocks;

  JSON_FIELDS(pre, blocks);
};

struct StateTransitionPost {
  lean::Slot slot;
  lean::Slot latest_block_header_slot;
  lean::BlockHash latest_block_header_state_root;
  size_t historical_block_hashes_count;

  JSON_FIELDS(slot,
              latest_block_header_slot,
              latest_block_header_state_root,
              historical_block_hashes_count);
};

struct StateTransitionResponse {
  bool succeeded;
  std::optional<std::string> error;
  std::optional<StateTransitionPost> post;

  JSON_FIELDS(succeeded, error, post);
};

template <typename RequestJson>
lean::http::Response httpJson(const lean::http::Request &request, auto &&f) {
  lean::http::Response response;
  RequestJson request_json;
  try {
    lean::json::decode(
        lean::json::NameCase::CAMEL, request_json, request.body());
  } catch (std::exception &e) {
    response.result(boost::beast::http::status::bad_request);
    response.body() = e.what();
    return response;
  }
  auto response_json = f(request_json);
  return lean::http::respondJson(
      lean::json::encode(lean::json::NameCase::CAMEL, response_json));
}

inline int cmdTestDriver(std::shared_ptr<lean::log::LoggingSystem> logsys,
                         boost::asio::ip::tcp::endpoint api_endpoint) {
  auto log = logsys->getLogger("TestDriver", "http");
  lean::http::ServerConfig config_api{
      .endpoint = api_endpoint,
      .on_request =
          [logsys, log](lean::http::Request request) {
            lean::http::Response response;
            std::string_view url{request.target()};
            SL_INFO(
                log, "{} {}", std::string_view{request.method_string()}, url);
            if (url == "/lean/v0/health") {
              return lean::http::respondJson(
                  R"({"status":"healthy","service":"lean-rpc-api"})");
            }
            if (url == "/lean/v0/test_driver/verify_signatures/run") {
              return httpJson<VerifySignaturesRequest>(
                  request, [&](VerifySignaturesRequest request) {
                    lean::ValidatorRegistry::ValidatorIndices validator_indices{
                        0};
                    auto validator_registry =
                        std::make_shared<lean::ValidatorRegistryMock>();
                    EXPECT_CALL(*validator_registry, currentValidatorIndices())
                        .Times(testing::AnyNumber())
                        .WillRepeatedly(testing::ReturnRef(validator_indices));
                    auto block_storage =
                        std::make_shared<lean::blockchain::BlockStorageMock>();
                    EXPECT_CALL(
                        *block_storage,
                        getState(request.signed_block.block.parent_root))
                        .WillOnce(testing::Return(request.anchor_state));
                    lean::ForkChoiceStore store{
                        {},
                        logsys,
                        std::make_shared<lean::metrics::MetricsMock>(),
                        {},
                        {},
                        {},
                        {},
                        {},
                        0,
                        validator_registry,
                        std::make_shared<
                            lean::app::ValidatorKeysManifestMock>(),
                        std::make_shared<
                            lean::crypto::xmss::XmssProviderImpl>(),
                        std::make_shared<lean::blockchain::BlockTreeMock>(),
                        block_storage,
                        false,
                        1,
                    };
                    return VerifySignaturesResponse{
                        .succeeded =
                            store.validateBlockSignatures(request.signed_block),
                    };
                  });
            }
            if (url == "/lean/v0/test_driver/state_transition/run") {
              return httpJson<StateTransitionRequest>(
                  request, [&](StateTransitionRequest request) {
                    auto block_tree =
                        std::make_shared<lean::blockchain::BlockTreeMock>();
                    EXPECT_CALL(*block_tree, getLatestJustified())
                        .Times(testing::AnyNumber());
                    EXPECT_CALL(*block_tree, lastFinalized())
                        .Times(testing::AnyNumber());
                    lean::STF stf{
                        logsys,
                        block_tree,
                        std::make_shared<lean::metrics::MetricsMock>(),
                    };
                    auto stf_many = [&]() -> outcome::result<lean::State> {
                      auto state = request.pre;
                      for (auto &block : request.blocks) {
                        BOOST_OUTCOME_TRY(
                            state, stf.stateTransition(block, state, true));
                      }
                      return state;
                    };
                    auto state_result = stf_many();
                    if (not state_result.has_value()) {
                      return StateTransitionResponse{
                          .succeeded = false,
                          .error = state_result.error().message(),
                      };
                    }
                    auto &state = state_result.value();
                    return StateTransitionResponse{
                        .succeeded = true,
                        .post =
                            StateTransitionPost{
                                .slot = state.slot,
                                .latest_block_header_slot =
                                    state.latest_block_header.slot,
                                .latest_block_header_state_root =
                                    state.latest_block_header.hash(),
                                .historical_block_hashes_count =
                                    state.historical_block_hashes.size(),
                            },
                    };
                  });
            }
            response.result(boost::beast::http::status::not_found);
            return response;
          },
  };
  boost::asio::io_context io_context;
  if (auto res = lean::http::serve(log, io_context, config_api);
      not res.has_value()) {
    SL_WARN(log,
            "listen api {}:{} error: {}",
            api_endpoint.address().to_string(),
            api_endpoint.port(),
            res.error());
    return EXIT_FAILURE;
  }
  SL_INFO(log,
          "listen api {}:{}",
          api_endpoint.address().to_string(),
          api_endpoint.port());
  io_context.run();
  return EXIT_SUCCESS;
}
