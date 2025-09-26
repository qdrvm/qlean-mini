/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <log/logger.hpp>
#include <modules/production/interfaces.hpp>
#include <qtils/create_smart_pointer_macros.hpp>

#include "types/config.hpp"
#include "types/validator_index.hpp"

namespace lean::app {
  class Configuration;
}  // namespace lean::app

namespace lean::blockchain {
  class BlockTree;
}  // namespace lean::blockchain

namespace lean::crypto {
  class Hasher;
}  // namespace lean::crypto

namespace lean::modules {

  class ProductionModuleImpl final : public lean::modules::ProductionModule {
    ProductionModuleImpl(lean::modules::ProductionLoader &loader,
                         qtils::SharedRef<lean::log::LoggingSystem> logsys,
                         qtils::SharedRef<app::Configuration> app_config,
                         qtils::SharedRef<blockchain::BlockTree> block_tree,
                         qtils::SharedRef<crypto::Hasher> hasher);

   public:
    CREATE_SHARED_METHOD(ProductionModuleImpl);

    void on_loaded_success() override;
    void on_loading_is_finished() override;

    void on_slot_started(std::shared_ptr<const messages::SlotStarted>) override;

    void on_leave_update(std::shared_ptr<const messages::NewLeaf>) override;
    void on_block_finalized(
        std::shared_ptr<const messages::Finalized>) override;

   private:
    outcome::result<void> propose(Slot slot, ValidatorIndex validator_index);

    lean::modules::ProductionLoader &loader_;
    qtils::SharedRef<lean::log::LoggingSystem> logsys_;
    lean::log::Logger logger_;
    qtils::SharedRef<app::Configuration> app_config_;
    qtils::SharedRef<blockchain::BlockTree> block_tree_;
    qtils::SharedRef<crypto::Hasher> hasher_;
    std::optional<Config> genesis_config_;
    std::vector<ValidatorIndex> validator_indices_;
  };


}  // namespace lean::modules
