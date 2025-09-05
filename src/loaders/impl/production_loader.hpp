/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/empty.hpp>

#include "loaders/loader.hpp"
#include "log/logger.hpp"
#include "modules/production/production.hpp"
#include "se/subscription.hpp"

namespace lean::loaders {

  class ProductionLoader final
      : public std::enable_shared_from_this<ProductionLoader>,
        public Loader,
        public modules::ProductionLoader {
    qtils::SharedRef<blockchain::BlockTree> block_tree_;
    qtils::SharedRef<crypto::Hasher> hasher_;

    std::shared_ptr<BaseSubscriber<qtils::Empty>> on_init_complete_;

    std::shared_ptr<BaseSubscriber<qtils::Empty>> on_loading_finished_;

    std::shared_ptr<
        BaseSubscriber<qtils::Empty,
                       std::shared_ptr<const messages::SlotStarted>>>
        on_slot_started_;
    std::shared_ptr<
        BaseSubscriber<qtils::Empty, std::shared_ptr<const messages::NewLeaf>>>
        on_leave_update_;
    std::shared_ptr<BaseSubscriber<qtils::Empty,
                                   std::shared_ptr<const messages::Finalized>>>
        on_block_finalized_;

   public:
    ProductionLoader(qtils::SharedRef<log::LoggingSystem> logsys,
                     qtils::SharedRef<Subscription> se_manager,
                     qtils::SharedRef<blockchain::BlockTree> block_tree,
                     qtils::SharedRef<crypto::Hasher> hasher)
        : Loader(std::move(logsys), std::move(se_manager)),
          block_tree_(std::move(block_tree)),
          hasher_(std::move(hasher)) {}

    ProductionLoader(const ProductionLoader &) = delete;
    ProductionLoader &operator=(const ProductionLoader &) = delete;

    ~ProductionLoader() override = default;

    void start(std::shared_ptr<modules::Module> module) override {
      set_module(module);
      auto module_accessor =
          get_module()
              ->getFunctionFromLibrary<std::weak_ptr<modules::ProductionModule>,
                                       modules::ProductionLoader &,
                                       std::shared_ptr<log::LoggingSystem>,
                                       std::shared_ptr<blockchain::BlockTree>,
                                       std::shared_ptr<crypto::Hasher>>(
                  "query_module_instance");

      if (not module_accessor) {
        return;
      }

      auto module_internal =
          (*module_accessor)(*this, logsys_, block_tree_, hasher_);

      on_init_complete_ = se::SubscriberCreator<qtils::Empty>::create<
          EventTypes::ProductionIsLoaded>(
          *se_manager_,
          SubscriptionEngineHandlers::kTest,
          [module_internal](auto &) {
            if (auto m = module_internal.lock()) {
              m->on_loaded_success();
            }
          });

      on_loading_finished_ = se::SubscriberCreator<qtils::Empty>::create<
          EventTypes::LoadingIsFinished>(
          *se_manager_,
          SubscriptionEngineHandlers::kTest,
          [module_internal](auto &) {
            if (auto m = module_internal.lock()) {
              m->on_loading_is_finished();
            }
          });

      on_slot_started_ =
          se::SubscriberCreator<qtils::Empty,
                                std::shared_ptr<const messages::SlotStarted>>::
              create<EventTypes::SlotStarted>(
                  *se_manager_,
                  SubscriptionEngineHandlers::kTest,
                  [module_internal](auto &, auto msg) {
                    if (auto m = module_internal.lock()) {
                      m->on_slot_started(std::move(msg));
                    }
                  });

      on_leave_update_ =
          se::SubscriberCreator<qtils::Empty,
                                std::shared_ptr<const messages::NewLeaf>>::
              create<EventTypes::BlockAdded>(
                  *se_manager_,
                  SubscriptionEngineHandlers::kTest,
                  [module_internal](auto &, auto msg) {
                    if (auto m = module_internal.lock()) {
                      m->on_leave_update(std::move(msg));
                    }
                  });

      on_block_finalized_ =
          se::SubscriberCreator<qtils::Empty,
                                std::shared_ptr<const messages::Finalized>>::
              create<EventTypes::BlockFinalized>(
                  *se_manager_,
                  SubscriptionEngineHandlers::kTest,
                  [module_internal](auto &, auto msg) {
                    if (auto m = module_internal.lock()) {
                      m->on_block_finalized(std::move(msg));
                    }
                  });

      se_manager_->notify(EventTypes::ProductionIsLoaded);
    }

    void dispatch_block_produced(std::shared_ptr<const Block> msg) override {
      se_manager_->notify(EventTypes::BlockProduced, msg);
    }
  };

}  // namespace lean::loaders
