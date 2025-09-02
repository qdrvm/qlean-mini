/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/outcome.hpp>

#include "lean_types/block.hpp"
#include "lean_types/block_body.hpp"
#include "lean_types/block_data.hpp"
#include "lean_types/block_header.hpp"
#include "lean_types/justification.hpp"
#include "lean_types/signed_block.hpp"
#include "lean_types/types.hpp"

namespace lean::blockchain {

  /**
   * A wrapper for a storage of blocks
   * Provides a convenient interface to work with it
   */
  class BlockStorage {
   public:
    virtual ~BlockStorage() = default;

    /**
     * Gets leaves of a block tree
     * @returns hashes of block tree leaves
     */
    [[nodiscard]] virtual outcome::result<std::vector<BlockHash>>
    getBlockTreeLeaves() const = 0;

    /**
     * Saves provided block tree leaves
     * @returns result of saving
     */
    virtual outcome::result<void> setBlockTreeLeaves(
        std::vector<BlockHash> leaves) = 0;

    /**
     * Get the last finalized block
     * @return BlockInfo of the block
     */
    [[nodiscard]] virtual outcome::result<BlockIndex> getLastFinalized()
        const = 0;

    // -- hash --

    /**
     * Adds slot-to-hash record for the provided block index to block
     * storage
     * @returns success or failure
     */
    virtual outcome::result<void> assignHashToSlot(
        const BlockIndex &block_index) = 0;

    /**
     * Removes slot-to-hash record for provided block index from block storage
     * @returns success or failure
     */
    virtual outcome::result<void> deassignHashToSlot(
        const BlockIndex &block_index) = 0;

    /**
     * Tries to get block hashes by slot
     * @returns vector of hashes or error
     */
    [[nodiscard]] virtual outcome::result<std::vector<BlockHash>> getBlockHash(
        BlockNumber slot) const = 0;

    // -- headers --

    /**
     * Check if header existing by provided block {@param block_hash}
     * @returns result or error
     */
    [[nodiscard]] virtual outcome::result<bool> hasBlockHeader(
        const BlockHash &block_hash) const = 0;

    /**
     * Saves block header to block storage
     * @returns hash of saved header or error
     */
    virtual outcome::result<BlockHash> putBlockHeader(
        const BlockHeader &header) = 0;

    /**
     * Tries to get a block header by hash
     * @returns block header or error
     */
    [[nodiscard]] virtual outcome::result<BlockHeader> getBlockHeader(
        const BlockHash &block_hash) const = 0;

    /**
     * Attempts to retrieve the block header for the given hash}.
     * @param block_hash The hash of the block whose header is to be retrieved.
     * @returns An optional containing the block header if found, std::nullopt
     * if not found, or an error if the operation fails.
     */
    [[nodiscard]] virtual outcome::result<std::optional<BlockHeader>>
    tryGetBlockHeader(const BlockHash &block_hash) const = 0;

    // -- body --

    /**
     * Saves provided body of block to  block storage
     * @returns result of saving
     */
    virtual outcome::result<void> putBlockBody(const BlockHash &block_hash,
                                               const BlockBody &block_body) = 0;

    /**
     * Tries to get block body
     * @returns block body or error
     */
    [[nodiscard]] virtual outcome::result<std::optional<BlockBody>>
    getBlockBody(const BlockHash &block_hash) const = 0;

    /**
     * Removes body of block with hash {@param block_hash} from block storage
     * @returns result of saving
     */
    virtual outcome::result<void> removeBlockBody(
        const BlockHash &block_hash) = 0;

    // -- justification --

    /**
     * Saves {@param justification} of block with hash {@param block_hash} to
     * block storage
     * @returns result of saving
     */
    virtual outcome::result<void> putJustification(
        const Justification &justification, const BlockHash &block_hash) = 0;

    /**
     * Tries to get justification of block finality by {@param block_hash}
     * @returns justification or error
     */
    virtual outcome::result<std::optional<Justification>> getJustification(
        const BlockHash &block_hash) const = 0;

    /**
     * Removes justification of block with hash {@param block_hash} from block
     * storage
     * @returns result of saving
     */
    virtual outcome::result<void> removeJustification(
        const BlockHash &block_hash) = 0;

    // -- combined

    /**
     * Saves block to block storage
     * @returns hash of saved header or error
     */
    virtual outcome::result<BlockHash> putBlock(const BlockData &block) = 0;

    /**
     * Tries to get block data
     * @returns block data or error
     */
    [[nodiscard]] virtual outcome::result<std::optional<SignedBlock>> getBlock(
        const BlockHash &block_hash) const = 0;

    /**
     * Removes all data of block by hash from block storage
     * @returns nothing or error
     */
    virtual outcome::result<void> removeBlock(const BlockHash &block_hash) = 0;
  };

}  // namespace lean::blockchain
