/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include "se/subscription.hpp"

namespace lean {
  class ForkChoiceStore;
}
namespace lean::log {
  class LoggingSystem;
}  // namespace lean::log

namespace lean::app {
  class Configuration;
  class Application;
}  // namespace lean::app

namespace lean::loaders {
  class Loader;
}  // namespace lean::loaders

namespace lean::modules {
  class Module;
}  // namespace lean::modules

namespace lean::injector {

  /**
   * Dependency injector for a universal node. Provides all major components
   * required by the JamNode application.
   */
  class NodeInjector final {
   public:
    explicit NodeInjector(std::shared_ptr<log::LoggingSystem> logging_system,
                          std::shared_ptr<app::Configuration> app_config);

    std::shared_ptr<app::Application> injectApplication();
    std::unique_ptr<loaders::Loader> register_loader(
        std::shared_ptr<modules::Module> module);

   protected:
    std::shared_ptr<class NodeInjectorImpl> pimpl_;
  };

}  // namespace lean::injector
