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

namespace lean::app {
  class Configuration;
}

namespace lean::blockchain {

  class AnchorStateImpl final : public AnchorState, Singleton<AnchorState> {
   public:
    AnchorStateImpl(const State &state) {
      static_cast<State &>(*this) = state;
    };

    AnchorStateImpl(const app::Configuration &app_config);

    BOOST_DI_INJECT_TRAITS(const app::Configuration &);
  };

}  // namespace lean::blockchain
