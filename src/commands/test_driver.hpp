/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include "app/chain_spec.hpp"
#include "blockchain/block_storage.hpp"
#include "blockchain/fork_choice.hpp"
#include "blockchain/impl/anchor_block_impl.hpp"
#include "blockchain/impl/anchor_state_impl.hpp"
#include "blockchain/validator_registry.hpp"
#include "clock/manual_clock.hpp"
#include "crypto/xmss/xmss_provider_impl.hpp"
#include "log/logger.hpp"
#include "metrics/metrics_mock.hpp"
#include "serde/json.hpp"
#include "types/fork_choice_test_json.hpp"
#include "types/signed_block.hpp"
#include "types/state.hpp"
#include "utils/http.hpp"

#define USING_(ns, name) using name = ns::name

#define MOCK_UNUSED                                                   \
  override {                                                          \
    throw std::logic_error{                                           \
        fmt::format("unused mock function {}", __PRETTY_FUNCTION__)}; \
  }

struct ValidatorRegistryMock : lean::ValidatorRegistry {
  const ValidatorIndices &currentValidatorIndices() const override {
    return current_validator_indices_;
  }
  ValidatorIndices allValidatorsIndices() const MOCK_UNUSED;
  std::optional<std::string> nodeIdByIndex(
      lean::ValidatorIndex index) const override {
    return std::format("node_{}", index);
  }
  std::optional<ValidatorIndices> validatorIndicesForNodeId(
      std::string_view node_id) const MOCK_UNUSED;

  ValidatorIndices current_validator_indices_{1};
};

struct ChainSpecMock : lean::app::ChainSpec {
  const lean::app::Bootnodes &getBootnodes() const MOCK_UNUSED;
  bool isAggregator() const override {
    return true;
  }
  bool setIsAggregator(bool is_aggregator) MOCK_UNUSED;
};

struct ConfigurationMock : lean::app::Configuration {
  uint64_t cliSubnetCount() const override {
    return 1;
  }
};

struct ValidatorKeysManifestMock : lean::app::ValidatorKeysManifest {
  std::optional<lean::crypto::xmss::XmssKeypair> getKeypair(
      const lean::crypto::xmss::XmssPublicKey &public_key) const override {
    return {};
  }
  std::vector<lean::crypto::xmss::XmssPublicKey> getAllXmssPubkeys()
      const override {
    return {};
  }
};

struct XmssProviderMock : lean::crypto::xmss::XmssProvider {
  lean::crypto::xmss::XmssKeypair generateKeypair(
      uint64_t activation_epoch, uint64_t num_active_epochs) MOCK_UNUSED;
  lean::crypto::xmss::XmssSignature sign(
      lean::crypto::xmss::XmssPrivateKey xmss_private_key,
      uint32_t epoch,
      const lean::crypto::xmss::XmssMessage &message) override {
    return {};
  }
  bool verify(
      const lean::crypto::xmss::XmssPublicKey &xmss_public_key,
      const lean::crypto::xmss::XmssMessage &message,
      uint32_t epoch,
      const lean::crypto::xmss::XmssSignature &xmss_signature) override {
    return true;
  }
  lean::crypto::xmss::XmssAggregatedSignature aggregateSignatures(
      std::span<const std::vector<lean::crypto::xmss::XmssPublicKey>>
          child_public_keys,
      std::span<const lean::crypto::xmss::XmssAggregatedSignature> child_proofs,
      std::span<const lean::crypto::xmss::XmssPublicKey> public_keys,
      std::span<const lean::crypto::xmss::XmssSignature> signatures,
      uint32_t epoch,
      const lean::crypto::xmss::XmssMessage &message) const override {
    return {};
  }
  bool verifyAggregatedSignatures(
      std::span<const lean::crypto::xmss::XmssPublicKey> public_keys,
      uint32_t epoch,
      const lean::crypto::xmss::XmssMessage &message,
      lean::crypto::xmss::XmssAggregatedSignatureIn aggregated_signature)
      const override {
    return true;
  }
};

struct BlockTreeMock : lean::blockchain::BlockTree {
  USING_(lean, BlockHash);
  USING_(lean, BlockHeader);
  USING_(lean, BlockIndex);
  USING_(lean, BlockBody);
  USING_(lean, SignedBlock);
  USING_(lean, Checkpoint);

  // BlockHeaderRepository
  outcome::result<lean::Slot> getSlotByHash(
      const BlockHash &block_hash) const override {
    return blocks_.at(block_hash).slot;
  }
  outcome::result<BlockHeader> getBlockHeader(
      const BlockHash &block_hash) const override {
    return blocks_.at(block_hash);
  }
  outcome::result<std::optional<BlockHeader>> tryGetBlockHeader(
      const BlockHash &block_hash) const override {
    return blocks_.at(block_hash);
  }

  // BlockTree
  const BlockHash &getGenesisBlockHash() const MOCK_UNUSED;
  bool has(const BlockHash &hash) const override {
    return blocks_.contains(hash);
  }
  outcome::result<BlockBody> getBlockBody(const BlockHash &) const MOCK_UNUSED;
  outcome::result<void> addBlockHeader(const BlockHeader &) MOCK_UNUSED;
  outcome::result<void> addBlockBody(const BlockHash &,
                                     const BlockBody &) MOCK_UNUSED;
  outcome::result<void> addExistingBlock(const BlockHash &,
                                         const BlockHeader &) MOCK_UNUSED;
  outcome::result<void> addBlock(SignedBlock block) override {
    auto header = block.block.getHeader();
    header.updateHash();
    blocks_.emplace(header.hash(), header);
    children_[header.parent_root].emplace_back(header.hash());
    return outcome::success();
  }
  outcome::result<void> removeLeaf(const BlockHash &) MOCK_UNUSED;
  outcome::result<void> finalize(const BlockHash &hash) override {
    last_finalized_ = blocks_.at(hash).index();
    return outcome::success();
  }
  outcome::result<void> setJustified(const BlockHash &hash) override {
    last_justified_ = blocks_.at(hash).index();
    return outcome::success();
  }
  outcome::result<std::vector<BlockHash>> getBestChainFromBlock(
      const BlockHash &, uint64_t) const MOCK_UNUSED;
  outcome::result<std::vector<BlockHash>> getDescendingChainToBlock(
      const BlockHash &, uint64_t) const MOCK_UNUSED;
  bool isFinalized(const BlockIndex &) const MOCK_UNUSED;
  BlockIndex bestBlock() const MOCK_UNUSED;
  outcome::result<BlockIndex> getBestContaining(const BlockHash &) const
      MOCK_UNUSED;
  std::vector<BlockHash> getLeaves() const MOCK_UNUSED;
  outcome::result<std::vector<BlockHash>> getChildren(
      const BlockHash &hash) const override {
    std::vector<BlockHash> children;
    if (auto it = children_.find(hash); it != children_.end()) {
      children = it->second;
    }
    return children;
  }
  BlockIndex lastFinalized() const override {
    return last_finalized_;
  }
  Checkpoint getLatestJustified() const override {
    return last_justified_;
  }
  outcome::result<std::optional<SignedBlock>> tryGetSignedBlock(
      const BlockHash &) const MOCK_UNUSED;

  lean::BlockIndex last_finalized_;
  lean::BlockIndex last_justified_;
  std::unordered_map<BlockHash, BlockHeader> blocks_;
  std::unordered_map<BlockHash, std::vector<BlockHash>> children_;
};

struct BlockStorageMock : lean::blockchain::BlockStorage {
  USING_(lean, BlockHash);
  USING_(lean, BlockHeader);
  USING_(lean, BlockBody);
  USING_(lean, BlockData);
  USING_(lean, BlockIndex);
  USING_(lean, Slot);
  USING_(lean, SignedBlock);
  USING_(lean, State);

  // BlockStorage
  outcome::result<void> setBlockTreeLeaves(std::vector<BlockHash>) MOCK_UNUSED;
  outcome::result<std::vector<BlockHash>> getBlockTreeLeaves() const
      MOCK_UNUSED;
  outcome::result<void> assignHashToSlot(const BlockIndex &) MOCK_UNUSED;
  outcome::result<void> deassignHashToSlot(const BlockIndex &) MOCK_UNUSED;
  outcome::result<std::vector<BlockHash>> getBlockHash(Slot) const MOCK_UNUSED;
  outcome::result<SlotIterator> seekLastSlot() const MOCK_UNUSED;
  outcome::result<bool> hasBlockHeader(const BlockHash &) const MOCK_UNUSED;
  outcome::result<BlockHash> putBlockHeader(const BlockHeader &) MOCK_UNUSED;
  outcome::result<BlockHeader> getBlockHeader(const BlockHash &) const
      MOCK_UNUSED;
  outcome::result<std::optional<BlockHeader>> tryGetBlockHeader(
      const BlockHash &) const MOCK_UNUSED;
  outcome::result<void> putBlockBody(const BlockHash &,
                                     const BlockBody &) MOCK_UNUSED;
  outcome::result<std::optional<BlockBody>> getBlockBody(
      const BlockHash &) const MOCK_UNUSED;
  outcome::result<void> removeBlockBody(const BlockHash &) MOCK_UNUSED;
  outcome::result<BlockHash> putBlock(const BlockData &) MOCK_UNUSED;
  outcome::result<void> putState(const BlockHash &block_hash,
                                 const State &state) override {
    states_.emplace(block_hash, state);
    return outcome::success();
  }
  outcome::result<std::optional<State>> getState(
      const BlockHash &block_hash) const override {
    return states_.at(block_hash);
  }
  outcome::result<void> removeState(const BlockHash &block_hash) MOCK_UNUSED;
  outcome::result<BlockData> getBlock(const BlockHash &,
                                      BlockParts) const MOCK_UNUSED;
  outcome::result<void> removeBlock(const BlockHash &) MOCK_UNUSED;
  outcome::result<SignedBlock> getSignedBlock(const BlockHash &) const
      MOCK_UNUSED;

  std::unordered_map<BlockHash, State> states_;
};

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
    anchor_state_ =
        std::make_shared<lean::blockchain::AnchorStateImpl>(init_state);
    anchor_block_ =
        std::make_shared<lean::blockchain::AnchorBlockImpl>(*anchor_state_);

    auto block_tree = std::make_shared<BlockTreeMock>();
    block_tree->last_finalized_ = anchor_block_->index();
    block_tree->last_justified_ = block_tree->last_finalized_;
    block_tree->blocks_.emplace(anchor_block_->hash(), *anchor_block_);

    auto block_storage = std::make_shared<BlockStorageMock>();
    block_storage->states_.emplace(anchor_block_->hash(), *anchor_state_);

    store_.emplace(anchor_state_,
                   anchor_block_,
                   std::make_shared<lean::clock::ManualClock>(),
                   logsys,
                   std::make_shared<lean::metrics::MetricsMock>(),
                   std::make_shared<ConfigurationMock>(),
                   std::make_shared<ValidatorRegistryMock>(),
                   std::make_shared<ChainSpecMock>(),
                   std::make_shared<ValidatorKeysManifestMock>(),
                   std::make_shared<XmssProviderMock>(),
                   block_tree,
                   block_storage);
    store_->dontPropose();
  }

  lean::ValidatorRegistry::ValidatorIndices validator_indices_{0};
  std::shared_ptr<lean::blockchain::AnchorStateImpl> anchor_state_;
  std::shared_ptr<lean::blockchain::AnchorBlockImpl> anchor_block_;
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
                    auto block_storage = std::make_shared<BlockStorageMock>();
                    block_storage->states_.emplace(
                        request.signed_block.block.parent_root,
                        request.anchor_state);
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
                        std::make_shared<ValidatorRegistryMock>(),
                        std::make_shared<ValidatorKeysManifestMock>(),
                        std::make_shared<
                            lean::crypto::xmss::XmssProviderImpl>(),
                        std::make_shared<BlockTreeMock>(),
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
                    lean::STF stf{
                        logsys,
                        std::make_shared<BlockTreeMock>(),
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
      .max_request_size = 16 << 20,
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
