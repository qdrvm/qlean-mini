/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <utils/ctor_limiters.hpp>

namespace lean::app {

  /// @class Application - Lean-application interface
  class Application : private Singleton<Application> {
   public:
    virtual ~Application() = default;

    /// Runs node
    virtual void run() = 0;
  };

}  // namespace lean::app
