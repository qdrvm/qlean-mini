/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */


#include "timeline_impl.hpp"

#include <iostream>
#include <utility>

#include "app/state_manager.hpp"
#include "blockchain/block_tree.hpp"
#include "blockchain/genesis_config.hpp"
#include "clock/clock.hpp"
#include "log/logger.hpp"
#include "modules/shared/networking_types.tmp.hpp"
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
                             qtils::SharedRef<GenesisConfig> config,
                             qtils::SharedRef<blockchain::BlockTree> block_tree)
      : logger_(logsys->getLogger("Timeline", "application")),
        state_manager_(std::move(state_manager)),
        config_(std::move(config)),
        clock_(std::move(clock)),
        se_manager_(std::move(se_manager)),
        block_tree_(std::move(block_tree)) {
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
    on_peers_total_count_updated_ = se::SubscriberCreator<
        qtils::Empty,
        std::shared_ptr<const messages::PeersTotalCountMessage>>::
        create<EventTypes::PeersTotalCountUpdated>(
            *se_manager_,
            SubscriptionEngineHandlers::kTest,
            [this](
                auto &,
                std::shared_ptr<const messages::PeersTotalCountMessage> msg) {
              connected_peers_ = msg->count;
            });
  }

  void TimelineImpl::start() {
    auto now = clock_->nowMsec();
    auto next_slot =
        now > config_->config.genesis_time * 1000
            ? (now - config_->config.genesis_time * 1000) / SLOT_DURATION_MS + 1
            : 1;
    auto time_to_next_slot = config_->config.genesis_time * 1000
                           + SLOT_DURATION_MS * next_slot - now;
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
    auto head = block_tree_->bestBlock();
    auto finalized = block_tree_->lastFinalized();
    auto justified = block_tree_->getLatestJustified();
    auto head_block_header_res = block_tree_->getBlockHeader(head.hash);

    BlockHash parent_root{};
    StateRoot state_root{};

    if (head_block_header_res) {
      parent_root = head_block_header_res.value().parent_root;
      state_root = head_block_header_res.value().state_root;
    }

    fmt::println(
        std::cerr,
        "+===============================================================+");
    fmt::println(std::cerr,
                 "  CHAIN STATUS: Current Slot: {} | Head Slot: {}",
                 msg->slot,
                 head.slot);
    fmt::println(std::cerr,
                 "+---------------------------------------------------------------+");
    fmt::println(std::cerr, "  Connected Peers:    {}",
                 connected_peers_.load());
    fmt::println(std::cerr,
                 "+---------------------------------------------------------------+");
    fmt::println(std::cerr, "  Head Block Root:    0x{}", head.hash.toHex());
    fmt::println(std::cerr, "  Parent Block Root:  0x{}", parent_root.toHex());
    fmt::println(std::cerr, "  State Root:         0x{}", state_root.toHex());
    fmt::println(
        std::cerr,
        "+---------------------------------------------------------------+");
    fmt::println(std::cerr,
                 "  Latest Justified:   Slot {:>6} | Root: 0x{}",
                 justified.slot,
                 justified.root.toHex());
    fmt::println(std::cerr,
                 "  Latest Finalized:   Slot {:>6} | Root: 0x{}",
                 finalized.slot,
                 finalized.hash.toHex());
    fmt::println(
        std::cerr,
        "+===============================================================+");

    SL_INFO(logger_, "⚡ Slot {} started", msg->slot);
    if (stopped_) [[unlikely]] {
      SL_INFO(logger_, "Timeline is stopped on slot {}", msg->slot);
      return;
    }

    auto now = clock_->nowMsec();
    auto next_slot =
        (now - config_->config.genesis_time * 1000) / SLOT_DURATION_MS + 1;
    auto time_to_next_slot = config_->config.genesis_time * 1000
                           + SLOT_DURATION_MS * next_slot - now;

    const auto slot_start_abs =
        config_->config.genesis_time * 1000
        + SLOT_DURATION_MS * msg->slot;  // in milliseconds

    auto abs_interval1 = slot_start_abs + SECONDS_PER_INTERVAL * 1000;
    auto abs_interval2 = slot_start_abs + 2 * SECONDS_PER_INTERVAL * 1000;
    auto abs_interval3 = slot_start_abs + 3 * SECONDS_PER_INTERVAL * 1000;

    auto ms_to_abs = [&](uint64_t abs_time_ms) -> uint64_t {
      return (abs_time_ms > now) ? (abs_time_ms - now) : 0ull;
    };

    // trigger interval 0 immediately
    se_manager_->notify(EventTypes::SlotIntervalStarted,
                        std::make_shared<const messages::SlotIntervalStarted>(
                            0, msg->slot, msg->epoch));

    // schedule other intervals and next slot
    auto time_to_interval_1 = ms_to_abs(abs_interval1);
    se_manager_->notifyDelayed(
        std::chrono::milliseconds(time_to_interval_1),
        EventTypes::SlotIntervalStarted,
        std::make_shared<const messages::SlotIntervalStarted>(
            1, msg->slot, msg->epoch));

    auto time_to_interval_2 = ms_to_abs(abs_interval2);
    se_manager_->notifyDelayed(
        std::chrono::milliseconds(time_to_interval_2),
        EventTypes::SlotIntervalStarted,
        std::make_shared<const messages::SlotIntervalStarted>(
            2, msg->slot, msg->epoch));

    auto time_to_interval_3 = ms_to_abs(abs_interval3);
    se_manager_->notifyDelayed(
        std::chrono::milliseconds(time_to_interval_3),
        EventTypes::SlotIntervalStarted,
        std::make_shared<const messages::SlotIntervalStarted>(
            3, msg->slot, msg->epoch));

    const auto next_slot_abs = config_->config.genesis_time * 1000
                             + SLOT_DURATION_MS * (msg->slot + 1);
    auto time_to_next_slot_abs = ms_to_abs(next_slot_abs);
    se_manager_->notifyDelayed(
        std::chrono::milliseconds(time_to_next_slot_abs),
        EventTypes::SlotStarted,
        std::make_shared<const messages::SlotStarted>(msg->slot + 1, 0, false));
  }

}  // namespace lean::app
