/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <atomic>
#include <chrono>
#include <string>

#include <qtils/outcome.hpp>
#include <qtils/shared_ref.hpp>
#include <utils/ctor_limiters.hpp>

#include "types/state.hpp"

namespace lean::app {
  class StateManager;
}

namespace lean {
  class AsioSslContext;
}

namespace lean {

  enum class StateSyncError : uint8_t {
    Busy = 1,
    Timeout,
    ShuttingDown,
    InvalidUrl,
    UnsupportedScheme,
    UserInfoIsNotSupported,
    Network,
    HttpBadStatus,
    DeserializeFailed,
    ValidationFailed
  };

  class StateSyncClient final : Singleton<StateSyncClient> {
   public:
    StateSyncClient(qtils::SharedRef<AsioSslContext> ssl_ctx,
                    qtils::SharedRef<app::StateManager> state_manager);

    void stop();

    outcome::result<State> fetch(const std::string &url,
                                 std::chrono::seconds timeout);

   private:
    qtils::SharedRef<AsioSslContext> ssl_ctx_;
    qtils::SharedRef<app::StateManager> state_manager_;
    std::atomic_flag is_shutting_down_ = ATOMIC_FLAG_INIT;
    std::atomic_flag busy_ = ATOMIC_FLAG_INIT;
  };

}  // namespace lean

OUTCOME_HPP_DECLARE_ERROR(lean, StateSyncError);
