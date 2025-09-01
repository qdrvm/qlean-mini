/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <csignal>
#include <thread>

#include "app/impl/state_manager_impl.hpp"
#include "testutil/prepare_loggers.hpp"

using lean::app::AppStateException;
using lean::app::StateManager;
using lean::app::StateManagerImpl;
using lean::log::LoggingSystem;
using OnPrepare = StateManager::OnPrepare;
using OnLaunch = StateManager::OnLaunch;
using OnShutdown = StateManager::OnShutdown;

using testing::Return;
using testing::Sequence;

class OnPrepareMock {
 public:
  MOCK_METHOD(bool, call, ());
  bool operator()() {
    return call();
  }
};
class OnLaunchMock {
 public:
  MOCK_METHOD(bool, call, ());
  bool operator()() {
    return call();
  }
};
class OnShutdownMock {
 public:
  MOCK_METHOD(void, call, ());
  void operator()() {
    return call();
  }
};

class StateManagerTest : public testing::Test {
 public:
  class StateManagerHacked : public StateManagerImpl {
   public:
    using StateManagerImpl::doLaunch;
    using StateManagerImpl::doPrepare;
    using StateManagerImpl::doShutdown;
    using StateManagerImpl::reset;
    using StateManagerImpl::state;
    using StateManagerImpl::StateManagerImpl;
  };

  void SetUp() override {
    app_state_manager =
        std::make_shared<StateManagerHacked>(testutil::prepareLoggers());
    app_state_manager->reset();
    prepare_cb = std::make_shared<OnPrepareMock>();
    launch_cb = std::make_shared<OnLaunchMock>();
    shutdown_cb = std::make_shared<OnShutdownMock>();
  }
  void TearDown() override {
    prepare_cb.reset();
    launch_cb.reset();
    shutdown_cb.reset();
  }

  std::shared_ptr<StateManagerHacked> app_state_manager;
  std::shared_ptr<OnPrepareMock> prepare_cb;
  std::shared_ptr<OnLaunchMock> launch_cb;
  std::shared_ptr<OnShutdownMock> shutdown_cb;
};

/**
 * @given new created StateManager
 * @when switch stages in order
 * @then state changes according to the order
 */
TEST_F(StateManagerTest, StateSequence_Normal) {
  ASSERT_EQ(app_state_manager->state(), StateManager::State::Init);
  ASSERT_NO_THROW(app_state_manager->doPrepare());
  ASSERT_EQ(app_state_manager->state(), StateManager::State::ReadyToStart);
  ASSERT_NO_THROW(app_state_manager->doLaunch());
  ASSERT_EQ(app_state_manager->state(), StateManager::State::Works);
  ASSERT_NO_THROW(app_state_manager->doShutdown());
  ASSERT_EQ(app_state_manager->state(), StateManager::State::ReadyToStop);
}

/**
 * @given StateManager in state 'ReadyToStart' (after stage 'prepare')
 * @when trying to run stage 'prepare' again
 * @then thrown exception, state wasn't change. Can to run stages 'launch' and
 * 'shutdown'
 */
TEST_F(StateManagerTest, StateSequence_Abnormal_1) {
  app_state_manager->doPrepare();
  EXPECT_THROW(app_state_manager->doPrepare(), AppStateException);
  EXPECT_EQ(app_state_manager->state(), StateManager::State::ReadyToStart);
  EXPECT_NO_THROW(app_state_manager->doLaunch());
  EXPECT_NO_THROW(app_state_manager->doShutdown());
}

/**
 * @given StateManager in state 'Works' (after stage 'launch')
 * @when trying to run stages 'prepare' and 'launch' again
 * @then thrown exceptions, state wasn't change. Can to run stages 'launch' and
 * 'shutdown'
 */
TEST_F(StateManagerTest, StateSequence_Abnormal_2) {
  app_state_manager->doPrepare();
  app_state_manager->doLaunch();
  EXPECT_THROW(app_state_manager->doPrepare(), AppStateException);
  EXPECT_THROW(app_state_manager->doLaunch(), AppStateException);
  EXPECT_EQ(app_state_manager->state(), StateManager::State::Works);
  EXPECT_NO_THROW(app_state_manager->doShutdown());
}

/**
 * @given StateManager in state 'ReadyToStop' (after stage 'shutdown')
 * @when trying to run any stages 'prepare' and 'launch' again
 * @then thrown exceptions, except shutdown. State wasn't change.
 */
TEST_F(StateManagerTest, StateSequence_Abnormal_3) {
  app_state_manager->doPrepare();
  app_state_manager->doLaunch();
  app_state_manager->doShutdown();
  EXPECT_THROW(app_state_manager->doPrepare(), AppStateException);
  EXPECT_THROW(app_state_manager->doLaunch(), AppStateException);
  EXPECT_THROW(app_state_manager->doShutdown(), AppStateException);
  EXPECT_EQ(app_state_manager->state(), StateManager::State::ReadyToStop);
}

/**
 * @given new created StateManager
 * @when add callbacks for each stages
 * @then done without exceptions
 */
TEST_F(StateManagerTest, AddCallback_Initial) {
  EXPECT_NO_THROW(app_state_manager->atPrepare([] {}));
  EXPECT_NO_THROW(app_state_manager->atLaunch([] {}));
  EXPECT_NO_THROW(app_state_manager->atShutdown([] {}));
}

/**
 * @given StateManager in state 'ReadyToStart' (after stage 'prepare')
 * @when add callbacks for each stages
 * @then thrown exception only for 'prepare' stage callback
 */
TEST_F(StateManagerTest, AddCallback_AfterPrepare) {
  app_state_manager->doPrepare();
  EXPECT_THROW(app_state_manager->atPrepare([] {}), AppStateException);
  EXPECT_NO_THROW(app_state_manager->atLaunch([] {}));
  EXPECT_NO_THROW(app_state_manager->atShutdown([] {}));
}

/**
 * @given StateManager in state 'Works' (after stage 'launch')
 * @when add callbacks for each stages
 * @then done without exception only for 'shutdown' stage callback
 */
TEST_F(StateManagerTest, AddCallback_AfterLaunch) {
  app_state_manager->doPrepare();
  app_state_manager->doLaunch();
  EXPECT_THROW(app_state_manager->atPrepare([] {}), AppStateException);
  EXPECT_THROW(app_state_manager->atLaunch([] {}), AppStateException);
  EXPECT_NO_THROW(app_state_manager->atShutdown([] {}));
}

/**
 * @given StateManager in state 'ReadyToStop' (after stage 'shutdown')
 * @when add callbacks for each stages
 * @then trown exceptions for each calls
 */
TEST_F(StateManagerTest, AddCallback_AfterShutdown) {
  app_state_manager->doPrepare();
  app_state_manager->doLaunch();
  app_state_manager->doShutdown();
  EXPECT_THROW(app_state_manager->atPrepare([] {}), AppStateException);
  EXPECT_THROW(app_state_manager->atLaunch([] {}), AppStateException);
  EXPECT_THROW(app_state_manager->atShutdown([] {}), AppStateException);
}

struct UnderControlObject {
  UnderControlObject(OnPrepareMock &p, OnLaunchMock &l, OnShutdownMock &s)
      : p(p), l(l), s(s) {}

  OnPrepareMock &p;
  OnLaunchMock &l;
  OnShutdownMock &s;
  int tag = 0;

  bool prepare() {
    tag = 1;
    return p();
  }

  bool start() {
    tag = 2;
    return l();
  }

  void stop() {
    tag = 3;
    s();
  }
};

/**
 * @given new created StateManager
 * @when register callbacks by reg() method
 * @then each callcack registered for appropriate state
 */
TEST_F(StateManagerTest, RegCallbacks) {
  UnderControlObject x(*prepare_cb, *launch_cb, *shutdown_cb);

  app_state_manager->takeControl(x);

  EXPECT_CALL(*prepare_cb, call()).WillOnce(Return(true));
  EXPECT_CALL(*launch_cb, call()).WillOnce(Return(true));
  EXPECT_CALL(*shutdown_cb, call()).WillOnce(Return());

  EXPECT_NO_THROW(app_state_manager->doPrepare());
  EXPECT_EQ(x.tag, 1);
  EXPECT_NO_THROW(app_state_manager->doLaunch());
  EXPECT_EQ(x.tag, 2);
  EXPECT_NO_THROW(app_state_manager->doShutdown());
  EXPECT_EQ(x.tag, 3);
}

/**
 * @given new created StateManager
 * @when register callbacks by reg() method and run() StateManager
 * @then each callcack executed according to the stages order
 */
TEST_F(StateManagerTest, Run_CallSequence) {
  app_state_manager.reset();  // Enforce destruction of previous instance
  app_state_manager =
      std::make_shared<StateManagerHacked>(testutil::prepareLoggers());

  UnderControlObject x(*prepare_cb, *launch_cb, *shutdown_cb);
  app_state_manager->takeControl(x);

  Sequence seq;
  EXPECT_CALL(*prepare_cb, call()).InSequence(seq).WillOnce(Return(true));
  EXPECT_CALL(*launch_cb, call()).InSequence(seq).WillOnce(Return(true));
  EXPECT_CALL(*shutdown_cb, call()).InSequence(seq).WillOnce(Return());

  app_state_manager->atLaunch([] {
    std::thread terminator([] {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      std::raise(SIGQUIT);
    });
    terminator.join();
    return true;
  });

  std::thread main([&] { EXPECT_NO_THROW(app_state_manager->run()); });
  main.join();
}
