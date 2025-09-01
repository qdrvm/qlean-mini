/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app/impl/state_manager_impl.hpp"

#include <csignal>
#include <functional>

#include "log/logger.hpp"

namespace lean::app {
  std::weak_ptr<StateManagerImpl> StateManagerImpl::wp_to_myself;

  std::atomic_bool StateManagerImpl::shutting_down_signals_enabled{false};

  void StateManagerImpl::shuttingDownSignalsEnable() {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = shuttingDownSignalsHandler;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGINT);
    sigaddset(&act.sa_mask, SIGTERM);
    sigaddset(&act.sa_mask, SIGQUIT);
    sigprocmask(SIG_BLOCK, &act.sa_mask, nullptr);
    sigaction(SIGINT, &act, nullptr);
    sigaction(SIGTERM, &act, nullptr);
    sigaction(SIGQUIT, &act, nullptr);
    shutting_down_signals_enabled.store(true);
    sigprocmask(SIG_UNBLOCK, &act.sa_mask, nullptr);
  }

  void StateManagerImpl::shuttingDownSignalsDisable() {
    auto expected = true;
    if (not shutting_down_signals_enabled.compare_exchange_strong(expected,
                                                                  false)) {
      return;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_DFL;
    sigaction(SIGINT, &act, nullptr);
    sigaction(SIGTERM, &act, nullptr);
    sigaction(SIGQUIT, &act, nullptr);
  }

  void StateManagerImpl::shuttingDownSignalsHandler(int signal) {
    shuttingDownSignalsDisable();
    if (auto self = wp_to_myself.lock()) {
      SL_TRACE(self->logger_, "Shutdown signal {} received", signal);
      self->shutdown();
    }
  }

  std::atomic_bool StateManagerImpl::log_rotate_signals_enabled{false};

  void StateManagerImpl::logRotateSignalsEnable() {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = logRotateSignalsHandler;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGHUP);
    sigprocmask(SIG_BLOCK, &act.sa_mask, nullptr);
    sigaction(SIGHUP, &act, nullptr);
    log_rotate_signals_enabled.store(true);
    sigprocmask(SIG_UNBLOCK, &act.sa_mask, nullptr);
  }

  void StateManagerImpl::logRotateSignalsDisable() {
    auto expected = true;
    if (not log_rotate_signals_enabled.compare_exchange_strong(expected,
                                                               false)) {
      return;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_DFL;
    sigaction(SIGHUP, &act, nullptr);
  }

  void StateManagerImpl::logRotateSignalsHandler(int signal) {
    if (auto self = wp_to_myself.lock()) {
      SL_TRACE(self->logger_, "Log rotate signal {} received", signal);
      self->logging_system_->doLogRotate();
    }
  }

  StateManagerImpl::StateManagerImpl(
      qtils::SharedRef<log::LoggingSystem> logging_system)
      : logger_(logging_system->getLogger("StateManager", "application")),
        logging_system_(std::move(logging_system)) {
    shuttingDownSignalsEnable();
    logRotateSignalsEnable();
    SL_TRACE(logger_, "Signal handlers set up");
  }

  StateManagerImpl::~StateManagerImpl() {
    shuttingDownSignalsDisable();
    logRotateSignalsDisable();
    wp_to_myself.reset();
  }

  void StateManagerImpl::reset() {
    std::lock_guard lg(mutex_);

    std::queue<OnPrepare> empty_prepare;
    std::swap(prepare_, empty_prepare);

    std::queue<OnLaunch> empty_launch;
    std::swap(launch_, empty_launch);

    std::queue<OnShutdown> empty_shutdown;
    std::swap(shutdown_, empty_shutdown);

    state_ = State::Init;
  }

  void StateManagerImpl::atPrepare(OnPrepare &&cb) {
    std::lock_guard lg(mutex_);
    if (state_ > State::Prepare) {
      throw AppStateException("adding callback for stage 'prepare'");
    }
    prepare_.emplace(std::move(cb));
  }

  void StateManagerImpl::atLaunch(OnLaunch &&cb) {
    std::lock_guard lg(mutex_);
    if (state_ > State::Starting) {
      throw AppStateException("adding callback for stage 'launch'");
    }
    launch_.emplace(std::move(cb));
  }

  void StateManagerImpl::atShutdown(OnShutdown &&cb) {
    std::lock_guard lg(mutex_);
    if (state_ > State::ShuttingDown) {
      throw AppStateException("adding callback for stage 'shutdown'");
    }
    shutdown_.emplace(std::move(cb));
  }

  void StateManagerImpl::doPrepare() {
    std::lock_guard lg(mutex_);

    auto state = State::Init;
    if (not state_.compare_exchange_strong(state, State::Prepare)) {
      if (state != State::ShuttingDown) {
        throw AppStateException("running stage 'preparing'");
      }
    }

    if (not prepare_.empty()) {
      SL_TRACE(logger_, "Running stage 'preparing'…");
    }

    while (!prepare_.empty()) {
      auto &cb = prepare_.front();
      if (state_ == State::Prepare) {
        auto success = cb();
        if (not success) {
          SL_ERROR(logger_, "Stage 'preparing' is failed");
          state = State::Prepare;
          state_.compare_exchange_strong(state, State::ShuttingDown);
        }
      }
      prepare_.pop();
    }

    state = State::Prepare;
    state_.compare_exchange_strong(state, State::ReadyToStart);
  }

  void StateManagerImpl::doLaunch() {
    std::lock_guard lg(mutex_);

    auto state = State::ReadyToStart;
    if (not state_.compare_exchange_strong(state, State::Starting)) {
      if (state != State::ShuttingDown) {
        throw AppStateException("running stage 'launch'");
      }
    }

    if (not launch_.empty()) {
      SL_TRACE(logger_, "Running stage 'launch'…");
    }

    while (!launch_.empty()) {
      auto &cb = launch_.front();
      if (state_.load() == State::Starting) {
        auto success = cb();
        if (not success) {
          SL_ERROR(logger_, "Stage 'launch' is failed");
          state = State::Starting;
          state_.compare_exchange_strong(state, State::ShuttingDown);
        }
      }
      launch_.pop();
    }

    state = State::Starting;
    state_.compare_exchange_strong(state, State::Works);
  }

  void StateManagerImpl::doShutdown() {
    std::lock_guard lg(mutex_);

    auto state = State::Works;
    if (not state_.compare_exchange_strong(state, State::ShuttingDown)) {
      if (state != State::ShuttingDown) {
        throw AppStateException("running stage 'shutting down'");
      }
    }

    std::queue<OnPrepare> empty_prepare;
    std::swap(prepare_, empty_prepare);

    std::queue<OnLaunch> empty_launch;
    std::swap(launch_, empty_launch);

    while (!shutdown_.empty()) {
      auto &cb = shutdown_.front();
      cb();
      shutdown_.pop();
    }

    state = State::ShuttingDown;
    state_.compare_exchange_strong(state, State::ReadyToStop);
  }

  void StateManagerImpl::run() {
    wp_to_myself = weak_from_this();
    if (wp_to_myself.expired()) {
      throw std::logic_error(
          "StateManager must be instantiated on shared pointer before run");
    }

    doPrepare();

    doLaunch();

    if (state_.load() == State::Works) {
      SL_TRACE(logger_, "All components started; waiting shutdown request…");
      shutdownRequestWaiting();
    }

    SL_TRACE(logger_, "Start doing shutdown…");
    doShutdown();
    SL_TRACE(logger_, "Shutdown is done");

    if (state_.load() != State::ReadyToStop) {
      throw std::logic_error(
          "StateManager is expected in stage 'ready to stop'");
    }
  }

  void StateManagerImpl::shutdownRequestWaiting() {
    std::unique_lock lock(cv_mutex_);
    cv_.wait(lock, [&] { return state_ == State::ShuttingDown; });
  }

  void StateManagerImpl::shutdown() {
    shuttingDownSignalsDisable();
    if (state_.load() == State::ReadyToStop) {
      SL_TRACE(logger_, "Shutting down requested, but app is ready to stop");
      return;
    }

    if (state_.load() == State::ShuttingDown) {
      SL_TRACE(logger_, "Shutting down requested, but it's in progress");
      return;
    }

    SL_TRACE(logger_, "Shutting down requested…");
    std::lock_guard lg(cv_mutex_);
    state_.store(State::ShuttingDown);
    cv_.notify_one();
  }
}  // namespace lean::app
