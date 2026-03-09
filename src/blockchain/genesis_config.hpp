/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/shared_ref.hpp>

#include "types/config.hpp"
#include "types/state.hpp"

namespace lean::app {
  class Configuration;
}  // namespace lean::app


namespace lean::log {
  class LoggingSystem;
}  // namespace lean::log

namespace lean {
  struct GenesisConfig : Config {
    GenesisConfig(const log::LoggingSystem &logsys, const AnchorState &state);

    size_t validator_count;
  };
}  // namespace lean
