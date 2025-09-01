/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "app/state_manager.hpp"

#include <condition_variable>
#include <mutex>
#include <queue>

#include <qtils/shared_ref.hpp>

#include "utils/ctor_limiters.hpp"

namespace soralog {
  class Logger;
}  // namespace soralog
namespace lean::log {
  class LoggingSystem;
}  // namespace lean::log

namespace lean::app {

  class StateManagerImpl  // left non-final on purpose to be accessible in tests
      : Singleton<StateManager>,
        public StateManager,
        public std::enable_shared_from_this<StateManagerImpl> {
   public:
    StateManagerImpl(qtils::SharedRef<log::LoggingSystem> logging_system);

    ~StateManagerImpl() override;

    void atPrepare(OnPrepare &&cb) override;
    void atLaunch(OnLaunch &&cb) override;
    void atShutdown(OnShutdown &&cb) override;

    void run() override;
    void shutdown() override;

    State state() const override {
      return state_;
    }

   protected:
    void reset();

    void doPrepare() override;
    void doLaunch() override;
    void doShutdown() override;

   private:
    static std::weak_ptr<StateManagerImpl> wp_to_myself;

    static std::atomic_bool shutting_down_signals_enabled;
    static void shuttingDownSignalsEnable();
    static void shuttingDownSignalsDisable();
    static void shuttingDownSignalsHandler(int);

    static std::atomic_bool log_rotate_signals_enabled;
    static void logRotateSignalsEnable();
    static void logRotateSignalsDisable();
    static void logRotateSignalsHandler(int);

    void shutdownRequestWaiting();

    qtils::SharedRef<soralog::Logger> logger_;
    qtils::SharedRef<log::LoggingSystem> logging_system_;

    std::atomic<State> state_ = State::Init;

    std::recursive_mutex mutex_;

    std::mutex cv_mutex_;
    std::condition_variable cv_;

    std::queue<OnPrepare> prepare_;
    std::queue<OnLaunch> launch_;
    std::queue<OnShutdown> shutdown_;
  };

}  // namespace lean::app
