/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <optional>
#include <stdexcept>

#include <qtils/shared_ref.hpp>

#include "modules/module.hpp"
#include "se/subscription_fwd.hpp"

namespace lean::log {
  class LoggingSystem;
}  // namespace lean::log

namespace lean::modules {
  class Module;
}  // namespace lean::modules

namespace lean::loaders {

  class Loader {
   public:
    Loader(const Loader &) = delete;
    Loader &operator=(const Loader &) = delete;

    Loader(std::shared_ptr<log::LoggingSystem> logsys,
           std::shared_ptr<Subscription> se_manager)
        : logsys_(std::move(logsys)), se_manager_(std::move(se_manager)) {}

    virtual ~Loader() = default;
    virtual void start(std::shared_ptr<modules::Module>) = 0;

    std::optional<const char *> module_info() {
      if (module_) {
        auto result =
            module_->getFunctionFromLibrary<const char *>("module_info");
        if (result) {
          return (*result)();
        }
      }
      return std::nullopt;
    }

    std::shared_ptr<log::LoggingSystem> get_logsys() {
      return logsys_;
    }

    std::shared_ptr<modules::Module> get_module() {
      if (!module_) {
        throw std::runtime_error("Module not set");
      }
      return module_;
    }

    void set_module(std::shared_ptr<modules::Module> module) {
      module_ = module;
    }

   protected:
    qtils::SharedRef<log::LoggingSystem> logsys_;
    qtils::SharedRef<Subscription> se_manager_;

   private:
    std::shared_ptr<modules::Module> module_;
  };
}  // namespace lean::loaders
