/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/impl/anchor_state_impl.hpp"

#include "app/configuration.hpp"
#include "app/state_manager.hpp"
#include "blockchain/genesis_config.hpp"
#include "modules/networking/ssl_context.hpp"
#include "modules/networking/state_sync_client.hpp"
#include "modules/production/read_config_yaml.hpp"
#include "qtils/value_or_raise.hpp"

namespace lean::blockchain {

  AnchorStateImpl::AnchorStateImpl(
      qtils::SharedRef<log::LoggingSystem> logsys,
      qtils::SharedRef<const app::Configuration> app_config,
      qtils::SharedRef<app::StateManager> app_state_mngr) {
    if (app_config->stateSyncUrl().has_value()) {
      const auto &state_sync_url = app_config->stateSyncUrl().value();
      auto logger = logsys->getLogger("StateSyncing", "networking");

      SL_INFO(logger,
              "Pre-syncing the lastest-finalized state required with URL: {}",
              state_sync_url);

      StateSyncClient state_sync_client{std::make_shared<AsioSslContext>(),
                                        std::move(app_state_mngr)};

      std::shared_ptr<State> state{};
      for (int attempt = 1; attempt <= 3; ++attempt) {
        auto state_res =
            state_sync_client.fetch(state_sync_url, std::chrono::seconds(600));
        if (state_res.has_error()) {
          SL_WARN(
              logger,
              "Attempt {} to fetch latest-finalized state from {} failed: {}",
              attempt,
              state_sync_url,
              state_res.error());
          if (attempt == 3) {
            SL_CRITICAL(logger,
                        "Cannot fetch latest-finalized state from {}",
                        state_sync_url);
            qtils::raise(state_res.error());
          }
        } else {
          SL_INFO(logger,
                  "State successfully acquired from URL: {}",
                  state_sync_url);
          static_cast<State &>(*this) = state_res.value();
          return;
        }
      }
    }

    static_cast<State &>(*this) =
        qtils::valueOrRaise(readConfigYaml(app_config->genesisConfigPath()));
  }

}  // namespace lean::blockchain
