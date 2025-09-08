/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */


#include "blockchain/impl/genesis_block_header_impl.hpp"

#include "app/chain_spec.hpp"
#include "crypto/hasher.hpp"
#include "log/logger.hpp"
#include "sszpp/ssz++.hpp"

namespace lean::blockchain {

  GenesisBlockHeaderImpl::GenesisBlockHeaderImpl(
      const qtils::SharedRef<log::LoggingSystem> &logsys,
      const qtils::SharedRef<app::ChainSpec> &chain_spec,
      const qtils::SharedRef<crypto::Hasher> &hasher) {
    auto res = encode(chain_spec->genesisHeader());
    if (res.has_error()) {
      auto logger = logsys->getLogger("ChainSpec", "application");
      SL_CRITICAL(logger,
                  "Failed to decode genesis block header from chain spec: {}",
                  res.error());
      qtils::raise_on_err(res);
    }
    updateHash();
  }

}  // namespace lean::blockchain
