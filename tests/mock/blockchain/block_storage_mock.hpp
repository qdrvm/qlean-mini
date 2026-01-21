/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <gmock/gmock.h>

#include "blockchain/block_storage.hpp"

namespace lean::blockchain {

  class BlockStorageMock : public BlockStorage {
   public:
    MOCK_METHOD(outcome::result<void>,
                setBlockTreeLeaves,
                (std::vector<BlockHash>),
                (override));

    MOCK_METHOD(outcome::result<std::vector<BlockHash>>,
                getBlockTreeLeaves,
                (),
                (const, override));

    MOCK_METHOD(outcome::result<void>,
                assignHashToSlot,
                (const BlockIndex &),
                (override));

    MOCK_METHOD(outcome::result<void>,
                deassignHashToSlot,
                (const BlockIndex &),
                (override));

    MOCK_METHOD(outcome::result<std::vector<BlockHash>>,
                getBlockHash,
                (Slot),
                (const, override));

    MOCK_METHOD(outcome::result<SlotIterator>, seekLastSlot, (), (const));

    MOCK_METHOD(outcome::result<bool>,
                hasBlockHeader,
                (const BlockHash &),
                (const, override));

    MOCK_METHOD(outcome::result<BlockHash>,
                putBlockHeader,
                (const BlockHeader &),
                (override));

    MOCK_METHOD(outcome::result<BlockHeader>,
                getBlockHeader,
                (const BlockHash &),
                (const, override));

    MOCK_METHOD(outcome::result<std::optional<BlockHeader>>,
                tryGetBlockHeader,
                (const BlockHash &),
                (const, override));

    MOCK_METHOD(outcome::result<void>,
                putBlockBody,
                (const BlockHash &, const BlockBody &),
                (override));

    MOCK_METHOD(outcome::result<std::optional<BlockBody>>,
                getBlockBody,
                (const BlockHash &),
                (const, override));

    MOCK_METHOD(outcome::result<void>,
                removeBlockBody,
                (const BlockHash &),
                (override));

    MOCK_METHOD(outcome::result<BlockHash>,
                putBlock,
                (const BlockData &),
                (override));

    MOCK_METHOD(outcome::result<void>,
                putState,
                (const BlockHash &block_hash, const State &state),
                (override));

    MOCK_METHOD(outcome::result<std::optional<State>>,
                getState,
                (const BlockHash &block_hash),
                (const, override));

    MOCK_METHOD(outcome::result<void>,
                removeState,
                (const BlockHash &block_hash),
                (override));

    MOCK_METHOD(outcome::result<BlockData>,
                getBlock,
                (const BlockHash &, BlockParts),
                (const, override));

    MOCK_METHOD(outcome::result<void>,
                removeBlock,
                (const BlockHash &),
                (override));

    MOCK_METHOD(outcome::result<SignedBlockWithAttestation>,
                getSignedBlockWithAttestation,
                (const BlockHash &),
                (const, override));
  };

}  // namespace lean::blockchain
