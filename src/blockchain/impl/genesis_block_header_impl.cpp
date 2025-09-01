/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */


#include "blockchain/impl/genesis_block_header_impl.hpp"

#include "app/chain_spec.hpp"
#include "crypto/hasher.hpp"
#include "log/logger.hpp"
#include "scale/jam_scale.hpp"

namespace lean::blockchain {

  GenesisBlockHeaderImpl::GenesisBlockHeaderImpl(
      const qtils::SharedRef<log::LoggingSystem> &logsys,
      const qtils::SharedRef<app::ChainSpec> &chain_spec,
      const qtils::SharedRef<crypto::Hasher> &hasher) {
    scale::impl::memory::DecoderFromSpan decoder(chain_spec->genesisHeader(), test_vectors::config::tiny);
    try {
      decode(static_cast<BlockHeader &>(*this), decoder);
    } catch (std::system_error &e) {
      auto logger = logsys->getLogger("ChainSpec", "application");
      SL_CRITICAL(logger,
                  "Failed to decode genesis block header from chain spec: {}",
                  e.code());
      qtils::raise(e.code());
    }
    updateHash(*hasher);
  }

}  // namespace lean::blockchain
