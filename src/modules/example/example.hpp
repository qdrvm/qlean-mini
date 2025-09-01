/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <log/logger.hpp>
#include <modules/example/interfaces.hpp>

namespace lean::modules {

  class ExampleModuleImpl final : public lean::modules::ExampleModule {
    lean::modules::ExampleModuleLoader &loader_;
    qtils::SharedRef<lean::log::LoggingSystem> logsys_;
    lean::log::Logger logger_;

   public:
    ExampleModuleImpl(lean::modules::ExampleModuleLoader &loader,
                      qtils::SharedRef<lean::log::LoggingSystem> logsys);

    void on_loaded_success() override;

    void on_loading_is_finished() override;

    void on_request(std::shared_ptr<const std::string> s) override;

    void on_response(std::shared_ptr<const std::string> s) override;

    void on_notify(std::shared_ptr<const std::string> s) override;
  };


}  // namespace lean::modules
