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

namespace lean {
  class ForkChoiceStore;
}  // namespace lean

namespace lean::blockchain {
  class BlockTree;
}  // namespace lean::blockchain

namespace lean::app {
  class ChainSpec;
  class Configuration;
}  // namespace lean::app

namespace lean::loaders {

  class NetworkingLoader final
      : public std::enable_shared_from_this<NetworkingLoader>,
        public Loader,
        public modules::NetworkingLoader {
    log::Logger logger_;
    qtils::SharedRef<metrics::Metrics> metrics_;
    qtils::SharedRef<blockchain::BlockTree> block_tree_;
    qtils::SharedRef<ForkChoiceStore> fork_choice_store_;
    qtils::SharedRef<app::ChainSpec> chain_spec_;
    qtils::SharedRef<app::Configuration> app_config_;

    std::shared_ptr<BaseSubscriber<qtils::Empty>> on_init_complete_;

    std::shared_ptr<BaseSubscriber<qtils::Empty>> on_loading_finished_;

    SimpleSubscription<messages::SendSignedBlock,
                       modules::Networking,
                       &modules::Networking::onSendSignedBlock>
        subscription_send_signed_block_;
    SimpleSubscription<messages::SendSignedVote,
                       modules::Networking,
                       &modules::Networking::onSendSignedVote>
        subscription_send_signed_vote_;

   public:
    NetworkingLoader(std::shared_ptr<log::LoggingSystem> logsys,
                     std::shared_ptr<Subscription> se_manager,
                     qtils::SharedRef<metrics::Metrics> metrics,
                     qtils::SharedRef<blockchain::BlockTree> block_tree,
                     qtils::SharedRef<ForkChoiceStore> fork_choice_store,
                     qtils::SharedRef<app::ChainSpec> chain_spec,
                     qtils::SharedRef<app::Configuration> app_config)
        : Loader(std::move(logsys), std::move(se_manager)),
          logger_(logsys_->getLogger("Networking", "networking_module")),
          metrics_{std::move(metrics)},
          block_tree_{std::move(block_tree)},
          fork_choice_store_{std::move(fork_choice_store)},
          chain_spec_{std::move(chain_spec)},
          app_config_{std::move(app_config)} {}

    NetworkingLoader(const NetworkingLoader &) = delete;
    NetworkingLoader &operator=(const NetworkingLoader &) = delete;

    ~NetworkingLoader() override = default;

    void start(std::shared_ptr<modules::Module> module) override {
      set_module(module);
      auto module_accessor =
          get_module()
              ->getFunctionFromLibrary<std::weak_ptr<modules::Networking>,
                                       modules::NetworkingLoader &,
                                       std::shared_ptr<log::LoggingSystem>,
                                       qtils::SharedRef<metrics::Metrics>,
                                       qtils::SharedRef<blockchain::BlockTree>,
                                       qtils::SharedRef<ForkChoiceStore>,
                                       qtils::SharedRef<app::ChainSpec>,
                                       qtils::SharedRef<app::Configuration>>(
                  "query_module_instance");

      if (not module_accessor) {
        return;
      }

      auto module_internal = (*module_accessor)(*this,
                                                logsys_,
                                                metrics_,
                                                block_tree_,
                                                fork_choice_store_,
                                                chain_spec_,
                                                app_config_);

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

      subscription_send_signed_block_.subscribe(*se_manager_, module_internal);
      subscription_send_signed_vote_.subscribe(*se_manager_, module_internal);

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

    void dispatch_peers_total_count_updated(
        std::shared_ptr<const messages::PeerCountsMessage> msg) override {
      SL_TRACE(logger_, "Dispatch PeersTotalCountUpdated");
      se_manager_->notify(lean::EventTypes::PeerCountsUpdated, msg);
    }

    void dispatchStatusMessageReceived(
        std::shared_ptr<const messages::StatusMessageReceived> message)
        override {
      SL_TRACE(logger_,
               "Dispatch StatusMessageReceived peer={} finalized={} head={}",
               message->from_peer,
               message->notification.finalized.slot,
               message->notification.head.slot);
      dispatchDerive(*se_manager_, message);
    }

    void dispatchSignedVoteReceived(
        std::shared_ptr<const messages::SignedVoteReceived> message) override {
      SL_TRACE(logger_, "Dispatch SignedVoteReceived");
      dispatchDerive(*se_manager_, message);
    }
  };
}  // namespace lean::loaders
