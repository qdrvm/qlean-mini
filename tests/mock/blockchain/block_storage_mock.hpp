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

    MOCK_METHOD(outcome::result<BlockInfo>,
                getLastFinalized,
                (),
                (const, override));

    MOCK_METHOD(outcome::result<void>,
                assignHashToSlot,
                (const BlockInfo &),
                (override));

    MOCK_METHOD(outcome::result<void>,
                deassignHashToSlot,
                (const BlockInfo &),
                (override));

    MOCK_METHOD(outcome::result<std::vector<BlockHash>>,
                getBlockHash,
                (Slot),
                (const, override));
    // MOCK_METHOD(outcome::result<std::optional<BlockHash>>,
    //             getBlockHash,
    //             (const BlockId &),
    //             (const, override));

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

    MOCK_METHOD(outcome::result<void>,
                putJustification,
                (const Justification &, const BlockHash &),
                (override));

    MOCK_METHOD(outcome::result<std::optional<Justification>>,
                getJustification,
                (const BlockHash &),
                (const, override));

    MOCK_METHOD(outcome::result<void>,
                removeJustification,
                (const BlockHash &),
                (override));

    MOCK_METHOD(outcome::result<BlockHash>,
                putBlock,
                (const Block &),
                (override));

    MOCK_METHOD(outcome::result<std::optional<BlockData>>,
                getBlockData,
                (const BlockHash &),
                (const, override));

    MOCK_METHOD(outcome::result<void>,
                removeBlock,
                (const BlockHash &),
                (override));
  };

}  // namespace lean::blockchain
