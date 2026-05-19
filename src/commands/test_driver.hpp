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
#include "blockchain/impl/anchor_block_impl.hpp"
#include "blockchain/impl/anchor_state_impl.hpp"
#include "crypto/xmss/xmss_provider_impl.hpp"
#include "log/logger.hpp"
#include "mock/app/chain_spec_mock.hpp"
#include "mock/app/configuration_mock.hpp"
#include "mock/app/validator_keys_manifest_mock.hpp"
#include "mock/blockchain/block_storage_mock.hpp"
#include "mock/blockchain/block_tree_mock.hpp"
#include "mock/blockchain/validator_registry_mock.hpp"
#include "mock/clock/manual_clock.hpp"
#include "mock/crypto/xmss_provider_mock.hpp"
#include "mock/metrics_mock.hpp"
#include "serde/json.hpp"
#include "types/fork_choice_test_json.hpp"
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

struct ForkChoiceInit {
  lean::State anchor_state;
  lean::Block anchor_block;

  JSON_FIELDS(anchor_state, anchor_block);
};

struct DriverSnapshot {
  lean::Slot head_slot;
  lean::BlockHash head_root;
  uint64_t time;
  lean::Checkpoint justified_checkpoint;
  lean::Checkpoint finalized_checkpoint;
  lean::BlockHash safe_target;

  JSON_FIELDS(head_slot,
              head_root,
              time,
              justified_checkpoint,
              finalized_checkpoint,
              safe_target);
};

struct StepResponse {
  bool accepted;
  std::optional<std::string> error;
  DriverSnapshot snapshot;

  JSON_FIELDS(accepted, error, snapshot);
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
  auto call = [&] { return f(request_json); };
  if constexpr (std::is_void_v<decltype(call())>) {
    call();
    response.result(boost::beast::http::status::no_content);
    return response;
  } else {
    auto response_json = call();
    return lean::http::respondJson(
        lean::json::encode(lean::json::NameCase::CAMEL, response_json));
  }
}

struct ForkChoiceDriver {
  using BlockHash = lean::BlockHash;

  ForkChoiceDriver(std::shared_ptr<lean::log::LoggingSystem> logsys,
                   lean::State init_state) {
    using testing::_;

    anchor_state_ =
        std::make_shared<lean::blockchain::AnchorStateImpl>(init_state);
    anchor_block_ =
        std::make_shared<lean::blockchain::AnchorBlockImpl>(*anchor_state_);

    auto app_config = std::make_shared<lean::app::ConfigurationMock>();
    EXPECT_CALL(*app_config, cliSubnetCount())
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::Return(1));

    auto validator_registry = std::make_shared<lean::ValidatorRegistryMock>();
    EXPECT_CALL(*validator_registry, currentValidatorIndices())
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::ReturnRef(validator_indices_));
    EXPECT_CALL(*validator_registry, nodeIdByIndex(_))
        .WillRepeatedly(
            [](lean::ValidatorIndex i) { return std::format("node_{}", i); });

    auto chain_spec = std::make_shared<lean::app::ChainSpecMock>();
    EXPECT_CALL(*chain_spec, isAggregator())
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::Return(true));

    auto validator_key_manifest =
        std::make_shared<lean::app::ValidatorKeysManifestMock>();
    EXPECT_CALL(*validator_key_manifest, getAllXmssPubkeys())
        .Times(testing::AnyNumber());
    EXPECT_CALL(*validator_key_manifest, getKeypair(_))
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::Return(std::nullopt));

    auto xmss = std::make_shared<lean::crypto::xmss::XmssProviderMock>();
    EXPECT_CALL(*xmss, verify(_, _, _, _))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*xmss, sign(_, _, _)).Times(testing::AnyNumber());
    EXPECT_CALL(*xmss, verifyAggregatedSignatures(_, _, _, _))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*xmss, aggregateSignatures(_, _, _, _, _, _))
        .Times(testing::AnyNumber());

    auto block_tree = std::make_shared<lean::blockchain::BlockTreeMock>();
    last_finalized_ = anchor_block_->index();
    last_justified_ = last_finalized_;
    blocks_.emplace(anchor_block_->hash(), *anchor_block_);
    EXPECT_CALL(*block_tree, lastFinalized()).WillRepeatedly([&] {
      return last_finalized_;
    });
    EXPECT_CALL(*block_tree, finalize(_)).WillRepeatedly([&](BlockHash hash) {
      last_finalized_ = blocks_.at(hash).index();
      return outcome::success();
    });
    EXPECT_CALL(*block_tree, getLatestJustified()).WillRepeatedly([&] {
      return last_justified_;
    });
    EXPECT_CALL(*block_tree, setJustified(_))
        .WillRepeatedly([&](BlockHash hash) {
          last_justified_ = blocks_.at(hash).index();
          return outcome::success();
        });
    EXPECT_CALL(*block_tree, has(_)).WillRepeatedly([&](BlockHash hash) {
      return blocks_.contains(hash);
    });
    EXPECT_CALL(*block_tree, getSlotByHash(_))
        .WillRepeatedly([&](BlockHash hash) { return blocks_.at(hash).slot; });
    EXPECT_CALL(*block_tree, getBlockHeader(_))
        .WillRepeatedly([&](BlockHash hash) { return blocks_.at(hash); });
    EXPECT_CALL(*block_tree, tryGetBlockHeader(_))
        .WillRepeatedly([&](BlockHash hash) { return blocks_.at(hash); });
    EXPECT_CALL(*block_tree, getChildren(_))
        .WillRepeatedly([&](BlockHash hash) { return children_[hash]; });
    EXPECT_CALL(*block_tree, addBlock(_))
        .WillRepeatedly([&](lean::SignedBlock block) {
          auto header = block.block.getHeader();
          header.updateHash();
          blocks_.emplace(header.hash(), header);
          children_[header.parent_root].emplace_back(header.hash());
          return outcome::success();
        });

    auto block_storage = std::make_shared<lean::blockchain::BlockStorageMock>();
    states_.emplace(anchor_block_->hash(), *anchor_state_);
    EXPECT_CALL(*block_storage, getState(_))
        .WillRepeatedly([&](BlockHash hash) { return states_.at(hash); });
    EXPECT_CALL(*block_storage, putState(_, _))
        .WillRepeatedly([&](BlockHash hash, lean::State state) {
          states_.emplace(hash, state);
          return outcome::success();
        });

    store_.emplace(anchor_state_,
                   anchor_block_,
                   std::make_shared<lean::clock::ManualClock>(),
                   logsys,
                   std::make_shared<lean::metrics::MetricsMock>(),
                   app_config,
                   validator_registry,
                   chain_spec,
                   validator_key_manifest,
                   xmss,
                   block_tree,
                   block_storage);
    store_->dontPropose();
  }

  lean::ValidatorRegistry::ValidatorIndices validator_indices_{0};
  lean::BlockIndex last_finalized_;
  lean::BlockIndex last_justified_;
  std::shared_ptr<lean::blockchain::AnchorStateImpl> anchor_state_;
  std::shared_ptr<lean::blockchain::AnchorBlockImpl> anchor_block_;
  std::unordered_map<BlockHash, lean::BlockHeader> blocks_;
  std::unordered_map<BlockHash, std::vector<BlockHash>> children_;
  std::unordered_map<BlockHash, lean::State> states_;
  std::optional<lean::ForkChoiceStore> store_;
};

inline int cmdTestDriver(std::shared_ptr<lean::log::LoggingSystem> logsys,
                         boost::asio::ip::tcp::endpoint api_endpoint) {
  auto log = logsys->getLogger("TestDriver", "http");
  auto fork_choice = std::make_shared<std::optional<ForkChoiceDriver>>();
  lean::http::ServerConfig config_api{
      .endpoint = api_endpoint,
      .on_request =
          [logsys, log, fork_choice](lean::http::Request request) {
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
            if (url == "/lean/v0/test_driver/fork_choice/init") {
              return httpJson<ForkChoiceInit>(
                  request, [&](ForkChoiceInit request) {
                    fork_choice->emplace(logsys, request.anchor_state);
                  });
            }
            if (url == "/lean/v0/test_driver/fork_choice/step") {
              if (not fork_choice->has_value()) {
                response.result(boost::beast::http::status::bad_request);
                response.body() =
                    "\"/lean/v0/test_driver/fork_choice/init\" was not called";
                return response;
              }
              auto &store = fork_choice->value().store_.value();
              return httpJson<lean::ForkChoiceStep>(
                  request, [&](lean::ForkChoiceStep request) {
                    std::optional<outcome::result<void>> result;
                    if (auto *tick_step =
                            std::get_if<lean::TickStep>(&request.v)) {
                      std::chrono::milliseconds time;
                      if (tick_step->interval.has_value()) {
                        time =
                            (**fork_choice)
                                .anchor_state_->config.genesisTimeMs()
                            + *tick_step->interval * lean::INTERVAL_DURATION_MS;
                      } else if (tick_step->time.has_value()) {
                        time = std::chrono::seconds{*tick_step->time};
                      } else {
                        throw std::runtime_error{
                            "TickStep no interval or time"};
                      }
                      result = [&]() -> outcome::result<void> {
                        store.onTick(time);
                        return outcome::success();
                      }();
                    } else if (auto *block_step =
                                   std::get_if<lean::BlockStep>(&request.v)) {
                      result = [&] {
                        auto &block = block_step->block;
                        lean::SignedBlock signed_block{
                            .block = block,
                            .signature = {},
                        };
                        for (auto &attestation : block.body.attestations) {
                          signed_block.signature.attestation_signatures
                              .push_back({.participants =
                                              attestation.aggregation_bits});
                        }
                        auto block_time =
                            std::chrono::seconds{store.getConfig().genesis_time}
                            + block.slot * lean::SLOT_DURATION_MS;
                        store.onTick(block_time);
                        return store.onBlock(signed_block);
                      }();
                      if (auto &checks = block_step->checks) {
                        if (checks->block_attestation_count) {
                          EXPECT_EQ(block_step->block.body.attestations.size(),
                                    *checks->block_attestation_count);
                        }
                        if (checks->block_attestations) {
                          auto &attestations =
                              block_step->block.body.attestations.data();
                          for (auto &check : *checks->block_attestations) {
                            lean::AggregationBits participants;
                            for (auto &validator : check.participants) {
                              participants.add(validator);
                            }
                            auto attestation_it = std::ranges::find_if(
                                attestations,
                                [&](const lean::AggregatedAttestation
                                        &attestation) {
                                  return attestation.aggregation_bits
                                      == participants;
                                });
                            EXPECT_NE(attestation_it, attestations.end());
                            auto &attestation = *attestation_it;
                            if (check.attestation_slot) {
                              EXPECT_EQ(attestation.data.slot,
                                        *check.attestation_slot);
                            }
                            if (check.target_slot) {
                              EXPECT_EQ(attestation.data.target.slot,
                                        *check.target_slot);
                            }
                          }
                        }
                      }
                    } else if (auto *attestation_step =
                                   std::get_if<lean::AttestationStep>(
                                       &request.v)) {
                      result = [&] {
                        return store.onGossipAttestation(
                            attestation_step->attestation);
                      }();
                    } else if (auto *aggregated_step = std::get_if<
                                   lean::GossipAggregatedAttestationStep>(
                                   &request.v)) {
                      result = [&] {
                        return store.onGossipAggregatedAttestation(
                            aggregated_step->attestation);
                      }();
                    }
                    if (not result.value().has_value()) {
                      return StepResponse{
                          .accepted = false,
                          .error = result->error().message(),
                      };
                    }
                    auto head = store.getHead();
                    return StepResponse{
                        .accepted = true,
                        .snapshot =
                            DriverSnapshot{
                                .head_slot = head.slot,
                                .head_root = head.root,
                                .time = store.time().interval,
                                .justified_checkpoint =
                                    store.getLatestJustified(),
                                .finalized_checkpoint =
                                    store.getLatestFinalized(),
                                .safe_target = store.getSafeTarget().root,
                            },
                    };
                  });
            }
            response.result(boost::beast::http::status::not_found);
            return response;
          },
      .max_request_size = 1 << 20,
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
