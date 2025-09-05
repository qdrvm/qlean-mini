/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

namespace lean::messages {
  struct SlotStarted;
  struct Finalized;
  struct NewLeaf;
}  // namespace lean::messages
namespace lean {
  struct Block;
}

namespace lean::modules {

  struct ProductionLoader {
    virtual ~ProductionLoader() = default;

    virtual void dispatch_block_produced(std::shared_ptr<const Block>) = 0;
  };

  struct ProductionModule {
    virtual ~ProductionModule() = default;
    virtual void on_loaded_success() = 0;
    virtual void on_loading_is_finished() = 0;

    virtual void on_slot_started(
        std::shared_ptr<const messages::SlotStarted>) = 0;

    virtual void on_leave_update(std::shared_ptr<const messages::NewLeaf>) = 0;

    virtual void on_block_finalized(
        std::shared_ptr<const messages::Finalized>) = 0;
  };

}  // namespace lean::modules
