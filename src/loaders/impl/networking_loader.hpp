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
#include "modules/networking/networking.hpp"
#include "modules/shared/networking_types.tmp.hpp"
#include "se/subscription.hpp"

namespace lean::loaders {

  class NetworkingLoader final
      : public std::enable_shared_from_this<NetworkingLoader>,
        public Loader,
        public modules::NetworkingLoader {
    log::Logger logger_;

    std::shared_ptr<BaseSubscriber<qtils::Empty>> on_init_complete_;

    std::shared_ptr<BaseSubscriber<qtils::Empty>> on_loading_finished_;

    std::shared_ptr<
        BaseSubscriber<qtils::Empty,
                       std::shared_ptr<const messages::BlockRequestMessage>>>
        on_block_request_;

   public:
    NetworkingLoader(std::shared_ptr<log::LoggingSystem> logsys,
                     std::shared_ptr<Subscription> se_manager)
        : Loader(std::move(logsys), std::move(se_manager)),
          logger_(logsys_->getLogger("Networking", "networking_module")) {}

    NetworkingLoader(const NetworkingLoader &) = delete;
    NetworkingLoader &operator=(const NetworkingLoader &) = delete;

    ~NetworkingLoader() override = default;

    void start(std::shared_ptr<modules::Module> module) override {
      set_module(module);
      auto module_accessor =
          get_module()
              ->getFunctionFromLibrary<std::weak_ptr<lean::modules::Networking>,
                                       modules::NetworkingLoader &,
                                       std::shared_ptr<log::LoggingSystem>>(
                  "query_module_instance");

      if (not module_accessor) {
        return;
      }

      auto module_internal = (*module_accessor)(*this, logsys_);

      on_init_complete_ = se::SubscriberCreator<qtils::Empty>::template create<
          EventTypes::NetworkingIsLoaded>(
          *se_manager_,
          SubscriptionEngineHandlers::kTest,
          [module_internal, this](auto &) {
            if (auto m = module_internal.lock()) {
              SL_TRACE(logger_, "Handle NetworkingIsLoaded");
              m->on_loaded_success();
            }
          });

      on_loading_finished_ =
          se::SubscriberCreator<qtils::Empty>::template create<
              EventTypes::LoadingIsFinished>(
              *se_manager_,
              SubscriptionEngineHandlers::kTest,
              [module_internal, this](auto &) {
                if (auto m = module_internal.lock()) {
                  SL_TRACE(logger_, "Handle LoadingIsFinished");
                  m->on_loading_is_finished();
                }
              });

      on_block_request_ = se::SubscriberCreator<
          qtils::Empty,
          std::shared_ptr<const messages::BlockRequestMessage>>::
          template create<EventTypes::BlockRequest>(
              *se_manager_,
              SubscriptionEngineHandlers::kTest,
              [module_internal, this](auto &, const auto &msg) {
                if (auto m = module_internal.lock()) {
                  SL_TRACE(
                      logger_, "Handle BlockRequest; rid={}", msg->ctx.rid);
                  m->on_block_request(msg);
                }
              });


      se_manager_->notify(lean::EventTypes::NetworkingIsLoaded);
    }

    void dispatch_peer_connected(
        std::shared_ptr<const messages::PeerConnectedMessage> msg) override {
      SL_TRACE(logger_, "Dispatch PeerConnected; peer={}", msg->peer);
      se_manager_->notify(lean::EventTypes::PeerConnected, msg);
    }

    void dispatch_peer_disconnected(
        std::shared_ptr<const messages::PeerDisconnectedMessage> msg) override {
      SL_TRACE(logger_, "Dispatch PeerDisconnected; peer={}", msg->peer);
      se_manager_->notify(lean::EventTypes::PeerDisconnected, msg);
    }

    void dispatch_block_announce(
        std::shared_ptr<const messages::BlockAnnounceMessage> msg) override {
      SL_TRACE(logger_, "Dispatch BlockAnnounceReceived");
      se_manager_->notify(lean::EventTypes::BlockAnnounceReceived, msg);
    }

    void dispatch_block_response(
        std::shared_ptr<const messages::BlockResponseMessage> msg) override {
      SL_TRACE(logger_, "Dispatch BlockResponse; rid={}", msg->ctx.rid);
      se_manager_->notify(lean::EventTypes::BlockResponse, std::move(msg));
    }
  };
}  // namespace lean::loaders
