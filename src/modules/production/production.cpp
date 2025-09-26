/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "modules/production/production.hpp"

#include "app/configuration.hpp"
#include "blockchain/block_tree.hpp"
#include "crypto/hasher.hpp"
#include "modules/production/read_config_yaml.hpp"
#include "modules/production/read_validators_yaml.hpp"
#include "modules/shared/networking_types.tmp.hpp"
#include "modules/shared/prodution_types.tmp.hpp"
#include "types/block_data.hpp"
#include "types/signed_block.hpp"

namespace lean::modules {
  ProductionModuleImpl::ProductionModuleImpl(
      ProductionLoader &loader,
      qtils::SharedRef<log::LoggingSystem> logging_system,
      qtils::SharedRef<app::Configuration> app_config,
      qtils::SharedRef<blockchain::BlockTree> block_tree,
      qtils::SharedRef<crypto::Hasher> hasher)
      : loader_(loader),
        logsys_(std::move(logging_system)),
        logger_(logsys_->getLogger("ProductionModule", "production_module")),
        app_config_{std::move(app_config)},
        block_tree_(std::move(block_tree)),
        hasher_(std::move(hasher)) {}

  void ProductionModuleImpl::on_loaded_success() {
    SL_INFO(logger_, "Loaded success");

    auto config_yaml_path = app_config_->basePath() / "genesis" / "config.yaml";
    auto config_yaml_result = readConfigYaml(config_yaml_path);
    if (not config_yaml_result.has_value()) {
      SL_ERROR(logger_,
               "Error reading {}: ",
               config_yaml_path.string(),
               config_yaml_result.error());
      return;
    }
    genesis_config_ = config_yaml_result.value();

    auto validators_yaml_path =
        app_config_->basePath() / "genesis" / "validators.yaml";
    auto validators_yaml_result = readValidatorsYaml(validators_yaml_path);
    if (not validators_yaml_result.has_value()) {
      SL_ERROR(logger_,
               "Error reading {}: ",
               validators_yaml_path.string(),
               validators_yaml_result.error());
      return;
    }
    auto &validators_yaml = validators_yaml_result.value();

    auto validators_it = validators_yaml.find(app_config_->nodeId());
    if (validators_it != validators_yaml.end()) {
      validator_indices_ = validators_it->second;
    }
  }

  void ProductionModuleImpl::on_loading_is_finished() {
    SL_INFO(logger_, "Loading is finished");
  }

  void ProductionModuleImpl::on_slot_started(
      std::shared_ptr<const messages::SlotStarted> msg) {
    if (msg->epoch_change) {
      SL_INFO(logger_, "Epoch changed to {}", msg->epoch);
    }

    SL_INFO(logger_, "Slot {} is started", msg->slot);

    for (auto &validator_index : validator_indices_) {
      auto producer_index = msg->slot % genesis_config_.value().num_validators;
      auto is_producer = validator_index == producer_index;
      if (is_producer) {
        auto res = propose(msg->slot, validator_index);
        if (not res.has_value()) {
          SL_INFO(logger_, "Propose error: {}", res.error());
        }
      }
    }
  }

  void ProductionModuleImpl::on_leave_update(
      std::shared_ptr<const messages::NewLeaf> msg) {
    SL_INFO(logger_,
            "New leaf {} appeared{}",
            msg->header.index(),
            msg->best ? "; it's the new best leaf" : "");
  }

  void ProductionModuleImpl::on_block_finalized(
      std::shared_ptr<const messages::Finalized> msg) {
    SL_INFO(logger_, "Chain finalized on block {}", msg->finalized);
    for (auto retired : msg->retired) {
      SL_INFO(logger_, "Block {} is retired", retired);
    }
  }

  outcome::result<void> ProductionModuleImpl::propose(
      Slot slot, ValidatorIndex validator_index) {
    SL_INFO(
        logger_, "Proposing in slot {} as validator {}", slot, validator_index);

    auto parent_hash = block_tree_->bestBlock().hash;
    // Produce block
    Block block;
    block.slot = slot;
    block.proposer_index = validator_index;
    block.parent_root = parent_hash;

    // Add a block into the block tree
    BOOST_OUTCOME_TRY(block_tree_->addBlock(block));

    // Notify subscribers
    loader_.dispatch_block_produced(std::make_shared<const Block>(block));

    // TODO(turuslan): signature
    loader_.dispatchSendSignedBlock(std::make_shared<messages::SendSignedBlock>(
        SignedBlock{.message = block}));

    return outcome::success();
  }
}  // namespace lean::modules
