/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <gmock/gmock.h>

#include "app/bootnodes.hpp"
#include "app/chain_spec.hpp"

namespace lean::app {

  class ChainSpecMock final : public ChainSpec {
   public:
    MOCK_METHOD(const std::string &, id, (), (const, override));

    MOCK_METHOD(const app::Bootnodes &,
                getBootnodes,
                (),
                (const, override));

    MOCK_METHOD(const qtils::ByteVec &, genesisHeader, (), (const, override));

    using KVMap = std::map<qtils::ByteVec, qtils::ByteVec>;
    MOCK_METHOD(const KVMap &, genesisState, (), (const, override));
  };

}  // namespace lean::app
