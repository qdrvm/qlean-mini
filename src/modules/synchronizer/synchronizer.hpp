/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <log/logger.hpp>
#include <modules/synchronizer/interfaces.hpp>
#include <qtils/create_smart_pointer_macros.hpp>
#include <qtils/shared_ref.hpp>
#include <utils/ctor_limiters.hpp>

namespace lean::modules {

  class SynchronizerImpl final : public Singleton<Synchronizer>,
                                 public Synchronizer {
    SynchronizerImpl(SynchronizerLoader &loader,
                     qtils::SharedRef<log::LoggingSystem> logging_system);

   public:
    CREATE_SHARED_METHOD(SynchronizerImpl);


    void on_loaded_success() override;

    void on_block_index_discovered(
        std::shared_ptr<const messages::BlockDiscoveredMessage> msg) override;

   private:
    SynchronizerLoader &loader_;
    log::Logger logger_;
  };

}  // namespace lean::modules
