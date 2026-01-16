/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/impl/anchor_state_impl.hpp"

#include "app/configuration.hpp"
#include "blockchain/genesis_config.hpp"
#include "modules/production/read_config_yaml.hpp"
#include "qtils/value_or_raise.hpp"

namespace lean::blockchain {

  AnchorStateImpl::AnchorStateImpl(const app::Configuration &app_config) {
    static_cast<State &>(*this) =
        qtils::valueOrRaise(readConfigYaml(app_config.genesisConfigPath()));
  }

}  // namespace lean::blockchain
