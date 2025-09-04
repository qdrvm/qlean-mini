/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#define BOOST_DI_CFG_DIAGNOSTICS_LEVEL 2
#define BOOST_DI_CFG_CTOR_LIMIT_SIZE 32

#include "injector/node_injector.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/di.hpp>
#include <boost/di/extension/scopes/shared.hpp>
#include <loaders/impl/example_loader.hpp>
#include <loaders/impl/networking_loader.hpp>
#include <loaders/impl/production_loader.hpp>
#include <loaders/impl/synchronizer_loader.hpp>

#include "app/configuration.hpp"
#include "app/impl/application_impl.hpp"
#include "app/impl/chain_spec_impl.hpp"
#include "app/impl/state_manager_impl.hpp"
#include "app/impl/timeline_impl.hpp"
#include "app/impl/watchdog.hpp"
#include "blockchain/impl/block_storage_impl.hpp"
#include "blockchain/impl/block_tree_impl.hpp"
#include "blockchain/impl/genesis_block_header_impl.hpp"
#include "clock/impl/clock_impl.hpp"
#include "crypto/hasher/hasher_impl.hpp"
#include "injector/bind_by_lambda.hpp"
#include "loaders/loader.hpp"
#include "log/logger.hpp"
#include "metrics/impl/exposer_impl.hpp"
#include "metrics/impl/prometheus/handler_impl.hpp"
#include "modules/module.hpp"
#include "se/impl/async_dispatcher_impl.hpp"
#include "se/subscription.hpp"
#include "storage/in_memory/in_memory_spaced_storage.hpp"
#include "storage/in_memory/in_memory_storage.hpp"
#include "storage/rocksdb/rocksdb.hpp"
#include "types/config.hpp"

namespace {
  namespace di = boost::di;
  namespace fs = std::filesystem;
  using namespace lean;  // NOLINT

  template <typename C>
  auto useConfig(C c) {
    return boost::di::bind<std::decay_t<C>>().to(
        std::move(c))[boost::di::override];
  }

  using injector::bind_by_lambda;

  template <typename... Ts>
  auto makeApplicationInjector(std::shared_ptr<log::LoggingSystem> logsys,
                               std::shared_ptr<app::Configuration> config,
                               Ts &&...args) {
    // clang-format off
    return di::make_injector(
        di::bind<app::Configuration>.to(config),
        di::bind<log::LoggingSystem>.to(logsys),
        di::bind<app::StateManager>.to<app::StateManagerImpl>(),
        di::bind<app::Application>.to<app::ApplicationImpl>(),
        di::bind<clock::SystemClock>.to<clock::SystemClockImpl>(),
        di::bind<clock::SteadyClock>.to<clock::SteadyClockImpl>(),
        di::bind<Watchdog>.to<Watchdog>(),
        di::bind<metrics::Handler>.to<metrics::PrometheusHandler>(),
        di::bind<metrics::Exposer>.to<metrics::ExposerImpl>(),
        di::bind<Dispatcher>.to<se::AsyncDispatcher<kHandlersCount, kThreadPoolSize>>(),
        di::bind<metrics::Exposer::Configuration>.to([](const auto &injector) {
          return metrics::Exposer::Configuration{
              injector
                  .template create<app::Configuration const &>()
                  .openmetricsHttpEndpoint()
          };
        }),
        di::bind<storage::BufferStorage>.to<storage::InMemoryStorage>(),
        //di::bind<storage::SpacedStorage>.to<storage::InMemorySpacedStorage>(),
        di::bind<storage::SpacedStorage>.to<storage::RocksDb>(),
        di::bind<app::ChainSpec>.to<app::ChainSpecImpl>(),
        di::bind<crypto::Hasher>.to<crypto::HasherImpl>(),
        di::bind<blockchain::GenesisBlockHeader>.to<blockchain::GenesisBlockHeaderImpl>(),
        di::bind<blockchain::BlockStorage>.to<blockchain::BlockStorageImpl>(),
        di::bind<app::Timeline>.to<app::TimelineImpl>(),
        di::bind<blockchain::BlockTree>.to<blockchain::BlockTreeImpl>(),

        // user-defined overrides...
        std::forward<decltype(args)>(args)...);
    // clang-format on
  }

  template <typename... Ts>
  auto makeNodeInjector(std::shared_ptr<log::LoggingSystem> logsys,
                        std::shared_ptr<app::Configuration> config,
                        Ts &&...args) {
    return di::make_injector<boost::di::extension::shared_config>(
        makeApplicationInjector(std::move(logsys), std::move(config)),

        // user-defined overrides...
        std::forward<decltype(args)>(args)...);
  }
}  // namespace

namespace lean::injector {
  class NodeInjectorImpl {
   public:
    using Injector =
        decltype(makeNodeInjector(std::shared_ptr<log::LoggingSystem>(),
                                  std::shared_ptr<app::Configuration>()));

    explicit NodeInjectorImpl(Injector injector)
        : injector_{std::move(injector)} {}

    Injector injector_;
  };

  NodeInjector::NodeInjector(std::shared_ptr<log::LoggingSystem> logsys,
                             std::shared_ptr<app::Configuration> config)
      : pimpl_{std::make_unique<NodeInjectorImpl>(
            makeNodeInjector(std::move(logsys), std::move(config)))} {}

  std::shared_ptr<app::Application> NodeInjector::injectApplication() {
    return pimpl_->injector_
        .template create<std::shared_ptr<app::Application>>();
  }

  std::unique_ptr<lean::loaders::Loader> NodeInjector::register_loader(
      std::shared_ptr<modules::Module> module) {
    auto logsys = pimpl_->injector_
                      .template create<std::shared_ptr<log::LoggingSystem>>();
    auto logger = logsys->getLogger("Modules", "lean");

    std::unique_ptr<lean::loaders::Loader> loader{};

    if ("ExampleLoader" == module->get_loader_id()) {
      loader = pimpl_->injector_
                   .create<std::unique_ptr<lean::loaders::ExampleLoader>>();
    } else if ("NetworkingLoader" == module->get_loader_id()) {
      loader = pimpl_->injector_
                   .create<std::unique_ptr<lean::loaders::NetworkingLoader>>();
    } else if ("ProductionLoader" == module->get_loader_id()) {
      loader = pimpl_->injector_
                   .create<std::unique_ptr<lean::loaders::ProductionLoader>>();
    } else if ("SynchronizerLoader" == module->get_loader_id()) {
      loader =
          pimpl_->injector_
              .create<std::unique_ptr<lean::loaders::SynchronizerLoader>>();
    } else {
      SL_CRITICAL(logger,
                  "> No loader found for: {} [{}]",
                  module->get_loader_id(),
                  module->get_path());
      return {};
    }

    loader->start(module);

    if (auto info = loader->module_info()) {
      SL_INFO(logger, "> Module: {} [{}]", *info, module->get_path());
    } else {
      SL_ERROR(logger,
               "> No module info for: {} [{}]",
               module->get_loader_id(),
               module->get_path());
    }
    return std::unique_ptr<lean::loaders::Loader>(loader.release());
  }
}  // namespace lean::injector
