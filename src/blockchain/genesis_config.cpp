/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/genesis_config.hpp"

#include <fmt/chrono.h>
#include <qtils/value_or_raise.hpp>

#include "app/configuration.hpp"
#include "log/logger.hpp"
#include "modules/production/read_config_yaml.hpp"

namespace lean {
  GenesisConfig::GenesisConfig(const log::LoggingSystem &logsys,
                               const app::Configuration &app_config)
      : state{qtils::valueOrRaise(
            readConfigYaml(app_config.genesisConfigPath()))},
        config{state.config} {
    auto logger = logsys.getLogger("GenesisConfig", "genesis_config");
    SL_INFO(logger,
            "Genesis config loaded: genesis_time={} (UTC {:%Y-%m-%d "
            "%H:%M:%S})",
            config.genesis_time,
            fmt::gmtime(config.genesis_time));
  }
}  // namespace lean
