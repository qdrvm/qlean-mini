/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <log/logger.hpp>
#include <modules/production/interfaces.hpp>
#include <qtils/create_smart_pointer_macros.hpp>

namespace lean {
  class ForkChoiceStoreMutex;
}  // namespace lean
namespace lean::clock {
  class SystemClock;
}  // namespace lean::clock
namespace lean::crypto {
  class Hasher;
}
namespace lean::blockchain {
  class BlockTree;
}

namespace lean::modules {

  class ProductionModuleImpl final : public lean::modules::ProductionModule {
    ProductionModuleImpl(
        lean::modules::ProductionLoader &loader,
        qtils::SharedRef<lean::log::LoggingSystem> logsys,
        qtils::SharedRef<blockchain::BlockTree> block_tree,
        std::shared_ptr<ForkChoiceStoreMutex> fork_choice_store,
        qtils::SharedRef<crypto::Hasher> hasher,
        qtils::SharedRef<clock::SystemClock> clock);

   public:
    CREATE_SHARED_METHOD(ProductionModuleImpl);

    void on_loaded_success() override;
    void on_loading_is_finished() override;

    void on_slot_interval_started(
        std::shared_ptr<const messages::SlotIntervalStarted>) override;

    void on_leave_update(std::shared_ptr<const messages::NewLeaf>) override;
    void on_block_finalized(
        std::shared_ptr<const messages::Finalized>) override;

   private:
    lean::modules::ProductionLoader &loader_;
    qtils::SharedRef<lean::log::LoggingSystem> logsys_;
    lean::log::Logger logger_;
    qtils::SharedRef<blockchain::BlockTree> block_tree_;
    qtils::SharedRef<ForkChoiceStoreMutex> fork_choice_store_;
    qtils::SharedRef<crypto::Hasher> hasher_;
    qtils::SharedRef<clock::SystemClock> clock_;
  };


}  // namespace lean::modules
