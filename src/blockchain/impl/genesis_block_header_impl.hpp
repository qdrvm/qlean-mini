/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/shared_ref.hpp>

#include "blockchain/genesis_block_header.hpp"

namespace lean::app {
  class ChainSpec;
}
namespace lean::log {
  class LoggingSystem;
}
namespace lean::crypto {
  class Hasher;
}

namespace lean::blockchain {

  class GenesisBlockHeaderImpl final : public GenesisBlockHeader {
   public:
    GenesisBlockHeaderImpl(const qtils::SharedRef<log::LoggingSystem> &logsys,
                           const qtils::SharedRef<app::ChainSpec> &chain_spec,
                           const qtils::SharedRef<crypto::Hasher> &hasher);
  };

}  // namespace lean::blockchain
