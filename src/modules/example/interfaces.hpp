/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

namespace lean::modules {

  struct ExampleModuleLoader {
    virtual ~ExampleModuleLoader() = default;

    virtual void dispatch_request(std::shared_ptr<const std::string>) = 0;
    virtual void dispatch_response(std::shared_ptr<const std::string>) = 0;
    virtual void dispatch_notify(std::shared_ptr<const std::string>) = 0;
  };

  struct ExampleModule {
    virtual ~ExampleModule() = default;
    virtual void on_loaded_success() = 0;
    virtual void on_loading_is_finished() = 0;

    virtual void on_request(std::shared_ptr<const std::string>) = 0;
    virtual void on_response(std::shared_ptr<const std::string>) = 0;
    virtual void on_notify(std::shared_ptr<const std::string>) = 0;
  };

}  // namespace lean::modules
