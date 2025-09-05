/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/shared_ref.hpp>
#include <types/types.hpp>
#include <utils/ctor_limiters.hpp>

namespace lean::log {
  class LoggingSystem;
}
namespace lean::storage {
  class SpacedStorage;
}
namespace lean::blockchain {
  class GenesisBlockHeader;
}
namespace lean::app {
  class ChainSpec;
}
namespace lean::crypto {
  class Hasher;
}

namespace lean::blockchain {

  class BlockStorageInitializer final : Singleton<BlockStorageInitializer> {
   public:
    BlockStorageInitializer(qtils::SharedRef<log::LoggingSystem> logsys,
                            qtils::SharedRef<storage::SpacedStorage> storage,
                            qtils::SharedRef<GenesisBlockHeader> genesis_header,
                            qtils::SharedRef<app::ChainSpec> chain_spec,
                            qtils::SharedRef<crypto::Hasher> hasher);
  };

}  // namespace lean::blockchain
