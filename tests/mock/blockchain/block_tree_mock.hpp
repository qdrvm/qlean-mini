/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <gmock/gmock.h>

#include "blockchain/block_tree.hpp"
#include "types/block_body.hpp"
#include "types/signed_block_with_attestation.hpp"

namespace lean::blockchain {

  class BlockTreeMock : public BlockTree {
   public:
    MOCK_METHOD(outcome::result<Slot>,
                getSlotByHash,
                (const BlockHash &block_hash),
                (const, override));
    MOCK_METHOD(outcome::result<BlockHeader>,
                getBlockHeader,
                (const BlockHash &block_hash),
                (const, override));
    MOCK_METHOD(outcome::result<std::optional<BlockHeader>>,
                tryGetBlockHeader,
                (const BlockHash &block_hash),
                (const, override));

    MOCK_METHOD(const BlockHash &, getGenesisBlockHash, (), (const, override));

    MOCK_METHOD(bool, has, (const BlockHash &hash), (const, override));

    MOCK_METHOD(outcome::result<BlockBody>,
                getBlockBody,
                (const BlockHash &block_hash),
                (const, override));

    MOCK_METHOD(outcome::result<void>,
                addBlockHeader,
                (const BlockHeader &header),
                (override));

    MOCK_METHOD(outcome::result<void>,
                addBlockBody,
                (const BlockHash &block_hash, const BlockBody &block_body),
                (override));

    MOCK_METHOD(outcome::result<void>,
                addExistingBlock,
                (const BlockHash &block_hash, const BlockHeader &block_header),
                (override));

    MOCK_METHOD(outcome::result<void>,
                addBlock,
                (SignedBlockWithAttestation signed_block_with_attestation),
                (override));

    MOCK_METHOD(outcome::result<void>,
                removeLeaf,
                (const BlockHash &block_hash),
                (override));

    MOCK_METHOD(outcome::result<void>,
                finalize,
                (const BlockHash &block),
                (override));

    MOCK_METHOD(outcome::result<std::vector<BlockHash>>,
                getBestChainFromBlock,
                (const BlockHash &block, uint64_t maximum),
                (const, override));

    MOCK_METHOD(outcome::result<std::vector<BlockHash>>,
                getDescendingChainToBlock,
                (const BlockHash &block, uint64_t maximum),
                (const, override));

    MOCK_METHOD(bool,
                isFinalized,
                (const BlockIndex &block),
                (const, override));

    MOCK_METHOD(BlockIndex, bestBlock, (), (const, override));

    MOCK_METHOD(outcome::result<BlockIndex>,
                getBestContaining,
                (const BlockHash &target_hash),
                (const, override));

    MOCK_METHOD(std::vector<BlockHash>, getLeaves, (), (const, override));

    MOCK_METHOD(outcome::result<std::vector<BlockHash>>,
                getChildren,
                (const BlockHash &block),
                (const, override));

    MOCK_METHOD(BlockIndex, lastFinalized, (), (const, override));
    MOCK_METHOD(Checkpoint, getLatestJustified, (), (const, override));

    MOCK_METHOD(outcome::result<std::optional<SignedBlockWithAttestation>>,
                tryGetSignedBlock,
                (const BlockHash block_hash),
                (const, override));

    MOCK_METHOD(void,
                import,
                (std::vector<SignedBlockWithAttestation> blocks),
                (override));
  };

}  // namespace lean::blockchain
