/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/empty.hpp>
#include <qtils/shared_ref.hpp>

#include "app/timeline.hpp"
#include "se/subscription_fwd.hpp"

namespace lean::messages {
  struct SlotStarted;
}
namespace lean {
  namespace log {
    class LoggingSystem;
  }
  struct Config;
}  // namespace lean
namespace lean::clock {
  class SystemClock;
}
namespace soralog {
  class Logger;
}
namespace lean::app {
  class Configuration;
  class StateManager;
}

namespace lean::app {

  class TimelineImpl final : public Timeline {
   public:
    TimelineImpl(qtils::SharedRef<log::LoggingSystem> logsys,
                 qtils::SharedRef<StateManager> state_manager,
                 qtils::SharedRef<Subscription> se_manager,
                 qtils::SharedRef<clock::SystemClock> clock,
                 qtils::SharedRef<Config> config);

    void prepare();
    void start();
    void stop();

   private:
    void on_slot_started(std::shared_ptr<const messages::SlotStarted> msg);

    qtils::SharedRef<soralog::Logger> logger_;
    qtils::SharedRef<StateManager> state_manager_;
    qtils::SharedRef<Config> config_;
    qtils::SharedRef<clock::SystemClock> clock_;
    qtils::SharedRef<Subscription> se_manager_;

    bool stopped_ = false;

    std::shared_ptr<
        BaseSubscriber<qtils::Empty,
                       std::shared_ptr<const messages::SlotStarted>>>
        on_slot_started_;
  };

}  // namespace lean::app
