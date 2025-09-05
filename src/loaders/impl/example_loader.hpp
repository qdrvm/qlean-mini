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
#include "modules/example/example.hpp"
#include "se/subscription.hpp"

namespace lean::loaders {

  class ExampleLoader final
      : public std::enable_shared_from_this<ExampleLoader>,
        public Loader,
        public modules::ExampleModuleLoader {
    std::shared_ptr<BaseSubscriber<qtils::Empty>> on_init_complete_;

    std::shared_ptr<BaseSubscriber<qtils::Empty>> on_loading_finished_;

    std::shared_ptr<
        BaseSubscriber<qtils::Empty, std::shared_ptr<const std::string>>>
        on_request_;

    std::shared_ptr<
        BaseSubscriber<qtils::Empty, std::shared_ptr<const std::string>>>
        on_response_;

    std::shared_ptr<
        BaseSubscriber<qtils::Empty, std::shared_ptr<const std::string>>>
        on_notification_;

   public:
    ExampleLoader(qtils::SharedRef<log::LoggingSystem> logsys,
                  qtils::SharedRef<Subscription> se_manager)
        : Loader(std::move(logsys), std::move(se_manager)) {}

    ExampleLoader(const ExampleLoader &) = delete;
    ExampleLoader &operator=(const ExampleLoader &) = delete;

    ~ExampleLoader() override = default;

    void start(std::shared_ptr<modules::Module> module) override {
      set_module(std::move(module));
      auto module_accessor =
          get_module()
              ->getFunctionFromLibrary<std::weak_ptr<modules::ExampleModule>,
                                       ExampleModuleLoader &,
                                       std::shared_ptr<log::LoggingSystem>>(
                  "query_module_instance");

      if (not module_accessor) {
        return;
      }

      auto module_internal = (*module_accessor)(*this, logsys_);

      on_init_complete_ = se::SubscriberCreator<qtils::Empty>::create<
          EventTypes::ExampleModuleIsLoaded>(
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

      on_request_ = se::SubscriberCreator<qtils::Empty,
                                          std::shared_ptr<const std::string>>::
          create<EventTypes::ExampleRequest>(
              *se_manager_,
              SubscriptionEngineHandlers::kTest,
              [module_internal](auto &, auto msg) {
                if (auto m = module_internal.lock()) {
                  m->on_request(std::move(msg));
                }
              });

      on_response_ = se::SubscriberCreator<qtils::Empty,
                                           std::shared_ptr<const std::string>>::
          create<EventTypes::ExampleResponse>(
              *se_manager_,
              SubscriptionEngineHandlers::kTest,
              [module_internal](auto &, auto msg) {
                if (auto m = module_internal.lock()) {
                  m->on_response(std::move(msg));
                }
              });

      on_notification_ =
          se::SubscriberCreator<qtils::Empty,
                                std::shared_ptr<const std::string>>::
              create<EventTypes::ExampleNotification>(
                  *se_manager_,
                  SubscriptionEngineHandlers::kTest,
                  [module_internal](auto &, auto msg) {
                    if (auto m = module_internal.lock()) {
                      m->on_notify(std::move(msg));
                    }
                  });

      se_manager_->notify(EventTypes::ExampleModuleIsLoaded);
    }

    void dispatch_request(std::shared_ptr<const std::string> msg) override {
      se_manager_->notify(EventTypes::ExampleRequest, msg);
    }

    void dispatch_response(std::shared_ptr<const std::string> msg) override {
      se_manager_->notify(EventTypes::ExampleResponse, msg);
    }

    void dispatch_notify(std::shared_ptr<const std::string> msg) override {
      se_manager_->notify(EventTypes::ExampleNotification, msg);
    }
  };
}  // namespace lean::loaders
