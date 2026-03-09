/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <modules/networking/networking.hpp>

#define MODULE_C_API extern "C" __attribute__((visibility("default")))
#define MODULE_API __attribute__((visibility("default")))

namespace lean::app {
  class ChainSpec;
  class Configuration;
}  // namespace lean::app

namespace lean::blockchain {
  class BlockTree;
}  // namespace lean::blockchain

namespace lean::metrics {
  class Metrics;
}  // namespace lean::metrics

MODULE_C_API const char *loader_id() {
  return "NetworkingLoader";
}

MODULE_C_API const char *module_info() {
  return "Networking v0.0";
}

static std::shared_ptr<lean::modules::Networking> module_instance;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type-c-linkage"

MODULE_C_API std::weak_ptr<lean::modules::Networking> query_module_instance(
    lean::modules::NetworkingLoader &loader,
    qtils::SharedRef<lean::log::LoggingSystem> logsys,
    qtils::SharedRef<lean::metrics::Metrics> metrics,
    qtils::SharedRef<lean::app::StateManager> app_state_mngr,
    qtils::SharedRef<lean::blockchain::BlockTree> block_tree,
    qtils::SharedRef<lean::ForkChoiceStore> fork_choice_store,
    qtils::SharedRef<lean::ValidatorRegistry> validator_registry,
    qtils::SharedRef<lean::GenesisConfig> genesis_config,
    qtils::SharedRef<lean::app::ChainSpec> chain_spec,
    qtils::SharedRef<lean::app::Configuration> app_config) {
  if (!module_instance) {
    module_instance =
        lean::modules::NetworkingImpl::create_shared(loader,
                                                     std::move(logsys),
                                                     std::move(metrics),
                                                     std::move(app_state_mngr),
                                                     block_tree,
                                                     fork_choice_store,
                                                     validator_registry,
                                                     genesis_config,
                                                     chain_spec,
                                                     app_config);
  }
  return module_instance;
}

MODULE_C_API void release_module_instance() {
  module_instance.reset();
}

#pragma GCC diagnostic pop
