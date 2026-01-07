/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/genesis_config.hpp"

#include <fmt/chrono.h>

#include "log/logger.hpp"

namespace lean {
  GenesisConfig::GenesisConfig(const log::LoggingSystem &logsys,
                               const AnchorState &state)
      : Config(state.config) {
    auto logger = logsys.getLogger("GenesisConfig", "genesis_config");
    SL_INFO(logger,
            "Genesis config loaded: "
            "genesis_time={} (UTC {:%Y-%m-%d %H:%M:%S})",
            genesis_time,
            fmt::gmtime(genesis_time));
  }
}  // namespace lean
