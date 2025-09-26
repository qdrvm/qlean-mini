/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */


#include "timeline_impl.hpp"

#include <utility>

#include "app/state_manager.hpp"
#include "clock/clock.hpp"
#include "log/logger.hpp"
#include "modules/shared/prodution_types.tmp.hpp"
#include "se/impl/subscription_manager.hpp"
#include "se/subscription.hpp"
#include "se/subscription_fwd.hpp"
#include "types/config.hpp"
#include "types/constants.hpp"

namespace lean::app {

  TimelineImpl::TimelineImpl(qtils::SharedRef<log::LoggingSystem> logsys,
                             qtils::SharedRef<StateManager> state_manager,
                             qtils::SharedRef<Subscription> se_manager,
                             qtils::SharedRef<clock::SystemClock> clock,
                             qtils::SharedRef<Config> config)
      : logger_(logsys->getLogger("Timeline", "application")),
        state_manager_(std::move(state_manager)),
        config_(std::move(config)),
        clock_(std::move(clock)),
        se_manager_(std::move(se_manager)) {
    state_manager_->takeControl(*this);
  }

  void TimelineImpl::prepare() {
    on_slot_started_ =
        se::SubscriberCreator<qtils::Empty,
                              std::shared_ptr<const messages::SlotStarted>>::
            create<EventTypes::SlotStarted>(
                *se_manager_,
                SubscriptionEngineHandlers::kTest,
                [this](auto &,
                       std::shared_ptr<const messages::SlotStarted> msg) {
                  on_slot_started(std::move(msg));
                });
  }

  void TimelineImpl::start() {
    auto now = clock_->nowMsec();
    auto next_slot = now > config_->genesis_time  // somehow now could be less
                                                  // than genesis time
                       ? (now - config_->genesis_time) / SLOT_DURATION_MS + 1
                       : 1;
    auto time_to_next_slot =
        config_->genesis_time + SLOT_DURATION_MS * next_slot - now;
    if (time_to_next_slot < SLOT_DURATION_MS / 2) {
      ++next_slot;
      time_to_next_slot += SLOT_DURATION_MS;
    }
    SL_INFO(logger_,
            "Starting timeline. Next slot is {}, starts in {}ms",
            next_slot,
            time_to_next_slot);
    se_manager_->notifyDelayed(
        std::chrono::milliseconds(time_to_next_slot),
        EventTypes::SlotStarted,
        std::make_shared<const messages::SlotStarted>(next_slot, 0, false));
  }

  void TimelineImpl::stop() {
    stopped_ = true;
  }

  void TimelineImpl::on_slot_started(
      std::shared_ptr<const messages::SlotStarted> msg) {
    if (stopped_) [[unlikely]] {
      SL_INFO(logger_, "Timeline is stopped on slot {}", msg->slot);
      return;
    }

    auto now = clock_->nowMsec();
    auto next_slot = (now - config_->genesis_time) / SLOT_DURATION_MS + 1;
    auto time_to_next_slot =
        config_->genesis_time + SLOT_DURATION_MS * next_slot - now;

    SL_INFO(logger_, "Next slot is {} in {}ms", next_slot, time_to_next_slot);

    auto time_to_interval_1 = SLOT_DURATION_MS / 4;
    se_manager_->notifyDelayed(
        std::chrono::milliseconds(time_to_interval_1),
        EventTypes::SlotIntervalOneStarted,
        std::make_shared<const messages::SlotIntervalOneStarted>(msg->slot,
                                                                 msg->epoch));

    auto time_to_interval_2 = SLOT_DURATION_MS / 2;
    se_manager_->notifyDelayed(
        std::chrono::milliseconds(time_to_interval_2),
        EventTypes::SlotIntervalTwoStarted,
        std::make_shared<const messages::SlotIntervalTwoStarted>(msg->slot,
                                                                 msg->epoch));

    auto time_to_interval_3 = 3 * SLOT_DURATION_MS / 4;
    se_manager_->notifyDelayed(
        std::chrono::milliseconds(time_to_interval_3),
        EventTypes::SlotIntervalThreeStarted,
        std::make_shared<const messages::SlotIntervalThreeStarted>(msg->slot,
                                                                   msg->epoch));

    se_manager_->notifyDelayed(
        std::chrono::milliseconds(time_to_next_slot),
        EventTypes::SlotStarted,
        std::make_shared<const messages::SlotStarted>(msg->slot + 1, 0, false));
  }

}  // namespace lean::app
