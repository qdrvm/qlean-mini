/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <log/logger.hpp>
#include <modules/production/interfaces.hpp>

namespace lean::modules {

  class ProductionModuleImpl final : public lean::modules::ProductionModule {
    lean::modules::ProductionLoader &loader_;
    qtils::SharedRef<lean::log::LoggingSystem> logsys_;
    lean::log::Logger logger_;

   public:
    ProductionModuleImpl(lean::modules::ProductionLoader &loader,
                         qtils::SharedRef<lean::log::LoggingSystem> logsys);

    void on_loaded_success() override;
    void on_loading_is_finished() override;

    void on_slot_started(std::shared_ptr<const messages::SlotStarted>) override;

    void on_leave_update(std::shared_ptr<const messages::NewLeaf>) override;
    void on_block_finalized(
        std::shared_ptr<const messages::Finalized>) override;
  };


}  // namespace lean::modules
