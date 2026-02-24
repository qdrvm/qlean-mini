/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <boost/di.hpp>
#include <qtils/shared_ref.hpp>

#include "types/state.hpp"
#include "utils/ctor_limiters.hpp"

namespace lean::log {
  class LoggingSystem;
}
namespace lean::app {
  class StateManager;
  class Configuration;
}

namespace lean::blockchain {

  class AnchorStateImpl final : public AnchorState, Singleton<AnchorState> {
   public:
    template <typename StateT>
      requires std::same_as<std::remove_cvref_t<StateT>, State>
    explicit AnchorStateImpl(StateT &&state) {
      static_cast<State &>(*this) = std::forward<State>(state);
    };

    AnchorStateImpl(qtils::SharedRef<log::LoggingSystem> logsys,
                    qtils::SharedRef<const app::Configuration> app_config,
                    qtils::SharedRef<app::StateManager> app_state_mngr);
  };

}  // namespace lean::blockchain
