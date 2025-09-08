/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <modules/shared/networking_types.tmp.hpp>
#include <modules/shared/synchronizer_types.tmp.hpp>

namespace lean::modules {

  struct SynchronizerLoader {
    virtual ~SynchronizerLoader() = default;
  };

  struct Synchronizer {
    virtual ~Synchronizer() = default;
    virtual void on_loaded_success() = 0;

    /// New block discovered (i.e., by peer's state view update)
    virtual void on_block_index_discovered(
        std::shared_ptr<const messages::BlockDiscoveredMessage> msg) = 0;
  };

}  // namespace lean::modules
