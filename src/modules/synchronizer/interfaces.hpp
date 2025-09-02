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

    virtual void dispatch_block_request(
        std::shared_ptr<const messages::BlockRequestMessage> msg) = 0;
  };

  struct Synchronizer {
    virtual ~Synchronizer() = default;
    virtual void on_loaded_success() = 0;

    /// New block discovered by block announce
    /// Expected from a network subsystem
    virtual void on_block_announce(
        std::shared_ptr<const messages::BlockAnnounceMessage> msg) = 0;

    /// New block discovered (i.e., by peer's state view update)
    virtual void on_block_index_discovered(
        std::shared_ptr<const messages::BlockDiscoveredMessage> msg) = 0;

    /// BlockResponse has received
    virtual void on_block_response(
        std::shared_ptr<const messages::BlockResponseMessage> msg) = 0;
  };

}  // namespace lean::modules
