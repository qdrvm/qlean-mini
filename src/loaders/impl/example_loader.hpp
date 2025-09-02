/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <iostream>

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

    std::shared_ptr<BaseSubscriber<std::shared_ptr<const std::string>>>
        on_request_;

    std::shared_ptr<BaseSubscriber<std::shared_ptr<const std::string>>>
        on_response_;

    std::shared_ptr<BaseSubscriber<std::shared_ptr<const std::string>>>
        on_notification_;

   public:
    ExampleLoader(qtils::SharedRef<log::LoggingSystem> logsys,
                  qtils::SharedRef<Subscription> se_manager)
        : Loader(std::move(logsys), std::move(se_manager)) {}

    ExampleLoader(const ExampleLoader &) = delete;
    ExampleLoader &operator=(const ExampleLoader &) = delete;

    ~ExampleLoader() override = default;

    void start(std::shared_ptr<modules::Module> module) override {
      set_module(module);
      auto module_accessor =
          get_module()
              ->getFunctionFromLibrary<
                  std::weak_ptr<lean::modules::ExampleModule>,
                  modules::ExampleModuleLoader &,
                  std::shared_ptr<log::LoggingSystem>>("query_module_instance");

      if (not module_accessor) {
        return;
      }

      auto module_internal = (*module_accessor)(*this, logsys_);

      on_init_complete_ = se::SubscriberCreator<qtils::Empty>::template create<
          EventTypes::ExampleModuleIsLoaded>(
          *se_manager_,
          SubscriptionEngineHandlers::kTest,
          [module_internal](auto &) {
            if (auto m = module_internal.lock()) {
              m->on_loaded_success();
            }
          });

      on_loading_finished_ =
          se::SubscriberCreator<qtils::Empty>::template create<
              EventTypes::LoadingIsFinished>(
              *se_manager_,
              SubscriptionEngineHandlers::kTest,
              [module_internal](auto &) {
                if (auto m = module_internal.lock()) {
                  m->on_loading_is_finished();
                }
              });

      on_request_ = se::SubscriberCreator<std::shared_ptr<const std::string>>::
          template create<EventTypes::ExampleRequest>(
              *se_manager_,
              SubscriptionEngineHandlers::kTest,
              [module_internal](auto &msg) {
                if (auto m = module_internal.lock()) {
                  m->on_request(msg);
                }
              });

      on_response_ = se::SubscriberCreator<std::shared_ptr<const std::string>>::
          template create<EventTypes::ExampleResponse>(
              *se_manager_,
              SubscriptionEngineHandlers::kTest,
              [module_internal](auto &msg) {
                if (auto m = module_internal.lock()) {
                  m->on_response(msg);
                }
              });

      on_notification_ =
          se::SubscriberCreator<std::shared_ptr<const std::string>>::
              template create<EventTypes::ExampleNotification>(
                  *se_manager_,
                  SubscriptionEngineHandlers::kTest,
                  [module_internal](auto &msg) {
                    if (auto m = module_internal.lock()) {
                      m->on_notify(msg);
                    }
                  });

      se_manager_->notify(lean::EventTypes::ExampleModuleIsLoaded);
    }

    void dispatch_request(std::shared_ptr<const std::string> s) override {
      se_manager_->notify(lean::EventTypes::ExampleRequest, s);
    }

    void dispatch_response(std::shared_ptr<const std::string> s) override {
      se_manager_->notify(lean::EventTypes::ExampleResponse, s);
    }

    void dispatch_notify(std::shared_ptr<const std::string> s) override {
      se_manager_->notify(lean::EventTypes::ExampleNotification, s);
    }
  };
}  // namespace lean::loaders
