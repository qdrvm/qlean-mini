/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>
#include <vector>

#include <qtils/byte_vec.hpp>

#include "app/bootnodes.hpp"

namespace lean::app {

  class ChainSpec {
   public:
    virtual ~ChainSpec() = default;

    [[nodiscard]] virtual const app::Bootnodes &getBootnodes() const = 0;
  };

}  // namespace lean::app
