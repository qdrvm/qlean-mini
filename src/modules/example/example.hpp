/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <log/logger.hpp>
#include <modules/example/interfaces.hpp>
#include <qtils/create_smart_pointer_macros.hpp>

namespace lean::modules {

  class ExampleModuleImpl final : public lean::modules::ExampleModule {
    ExampleModuleImpl(lean::modules::ExampleModuleLoader &loader,
                      qtils::SharedRef<lean::log::LoggingSystem> logsys);

   public:
    CREATE_SHARED_METHOD(ExampleModuleImpl);

    void on_loaded_success() override;

    void on_loading_is_finished() override;

    void on_request(std::shared_ptr<const std::string> s) override;

    void on_response(std::shared_ptr<const std::string> s) override;

    void on_notify(std::shared_ptr<const std::string> s) override;

   private:
    lean::modules::ExampleModuleLoader &loader_;
    qtils::SharedRef<lean::log::LoggingSystem> logsys_;
    lean::log::Logger logger_;
  };


}  // namespace lean::modules
