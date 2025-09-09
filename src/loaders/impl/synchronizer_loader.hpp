/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/empty.hpp>
#include <soralog/logging_system.hpp>

#include "loaders/loader.hpp"
#include "log/logger.hpp"
#include "modules/synchronizer/synchronizer.hpp"
#include "se/subscription.hpp"

namespace lean::loaders {

  class SynchronizerLoader final
      : public std::enable_shared_from_this<SynchronizerLoader>,
        public Loader,
        public modules::SynchronizerLoader {
    log::Logger logger_;

    using InitCompleteSubscriber = BaseSubscriber<qtils::Empty>;
    std::shared_ptr<InitCompleteSubscriber> on_init_complete_;

   public:
    SynchronizerLoader(std::shared_ptr<log::LoggingSystem> logsys,
                       std::shared_ptr<Subscription> se_manager)
        : Loader(std::move(logsys), std::move(se_manager)),
          logger_(logsys_->getLogger("Synchronizer", "synchronizer_module")) {}

    SynchronizerLoader(const SynchronizerLoader &) = delete;
    SynchronizerLoader &operator=(const SynchronizerLoader &) = delete;

    ~SynchronizerLoader() override = default;

    void start(std::shared_ptr<modules::Module> module) override {
      set_module(module);
      auto module_accessor =
          get_module()
              ->getFunctionFromLibrary<std::weak_ptr<modules::Synchronizer>,
                                       modules::SynchronizerLoader &,
                                       std::shared_ptr<log::LoggingSystem>>(
                  "query_module_instance");

      if (not module_accessor) {
        return;
      }

      auto module_internal = (*module_accessor)(*this, logsys_);

      on_init_complete_ = se::SubscriberCreator<qtils::Empty>::create<
          EventTypes::SynchronizerIsLoaded>(
          *se_manager_,
          SubscriptionEngineHandlers::kTest,
          [module_internal, this](auto &) {
            if (auto m = module_internal.lock()) {
              SL_TRACE(logger_, "Handle SynchronizerIsLoaded");
              m->on_loaded_success();
            }
          });

      se_manager_->notify(EventTypes::SynchronizerIsLoaded);
    }
  };

}  // namespace lean::loaders
