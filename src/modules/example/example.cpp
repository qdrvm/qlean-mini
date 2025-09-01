/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "modules/example/example.hpp"

namespace lean::modules {
  ExampleModuleImpl::ExampleModuleImpl(
      ExampleModuleLoader &loader,
      qtils::SharedRef<log::LoggingSystem> logging_system)
      : loader_(loader),
        logsys_(std::move(logging_system)),
        logger_(logsys_->getLogger("ExampleModule", "example_module")) {}

  void ExampleModuleImpl::on_loaded_success() {
    SL_INFO(logger_, "Loaded success");
  }

  void ExampleModuleImpl::on_loading_is_finished() {
    SL_INFO(logger_, "Loading is finished");
  }

  void ExampleModuleImpl::on_request(std::shared_ptr<const std::string> s) {
    SL_INFO(logger_, "Received request: {}", *s);
  }

  void ExampleModuleImpl::on_response(std::shared_ptr<const std::string> s) {
    SL_INFO(logger_, "Received response: {}", *s);
  }

  void ExampleModuleImpl::on_notify(std::shared_ptr<const std::string> s) {
    SL_INFO(logger_, "Received notification: {}", *s);
  }

}  // namespace lean::modules
