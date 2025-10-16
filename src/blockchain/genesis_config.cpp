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
      : config{qtils::valueOrRaise(
            readConfigYaml(app_config.genesisConfigPath()))} {
    auto logger = logsys.getLogger("GenesisConfig", "genesis_config");
    SL_INFO(logger,
            "Genesis config loaded: genesis_time={} (UTC {:%Y-%m-%d "
            "%H:%M:%S}), num_validators={}",
            config.genesis_time,
            fmt::gmtime(config.genesis_time),
            config.num_validators);
  }
}  // namespace lean
