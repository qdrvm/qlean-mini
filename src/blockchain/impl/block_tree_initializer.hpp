/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/shared_ref.hpp>
#include <types/block_header.hpp>
#include <utils/ctor_limiters.hpp>

namespace lean::log {
  class LoggingSystem;
}
namespace lean::blockchain {
  class BlockStorage;
}

namespace lean::blockchain {

  class BlockTreeInitializer final : Singleton<BlockTreeInitializer> {
   public:
    BlockTreeInitializer(qtils::SharedRef<log::LoggingSystem> logsys,
                         qtils::SharedRef<BlockStorage> storage);

    std::tuple<BlockIndex, std::map<BlockIndex, BlockHeader>>
    nonFinalizedSubTree();

   private:
    std::atomic_flag used_;
    BlockIndex last_finalized_;
    std::map<BlockIndex, BlockHeader> non_finalized_;
  };

}  // namespace lean::blockchain
