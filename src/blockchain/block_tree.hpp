/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "blockchain/block_header_repository.hpp"
#include "types/justification.hpp"

namespace lean {
  struct Block;
  struct BlockBody;
  struct SignedBlockWithAttestation;
  struct StatusMessage;
  struct Checkpoint;
}  // namespace lean

namespace lean::blockchain {
  class BlockTree : public BlockHeaderRepository {
   public:
    [[nodiscard]] virtual const BlockHash &getGenesisBlockHash() const = 0;

    /**
     * Checks containing of block header by provided block header
     * @param hash hash of the block header we are checking
     * @return containing block header or does not, or error
     */
    [[nodiscard]] virtual bool has(const BlockHash &hash) const = 0;

    /**
     * Get a body of the block (if present)
     * @param block_hash hash of the block to get body for
     * @return body, if the block exists in our storage, error in case it does
     * not exist in our storage, or actual error happens
     */
    [[nodiscard]] virtual outcome::result<BlockBody> getBlockBody(
        const BlockHash &block_hash) const = 0;

    /**
     * Adds header to the storage
     * @param header that we are adding
     * @return result with success if the header's parent exists on storage and
     * a new header was added. Error otherwise
     */
    virtual outcome::result<void> addBlockHeader(const BlockHeader &header) = 0;

    /**
     * Adds block body to the storage
     * @param block_hash that corresponds to the block which body we are adding
     * @param block_body that we are adding
     * @return result with success if block body was inserted. Error otherwise
     */
    virtual outcome::result<void> addBlockBody(const BlockHash &block_hash,
                                               const BlockBody &block_body) = 0;

    /**
     * Add an existent block to the tree
     * @param block_hash is hash of the added block in the tree
     * @param block_header is header of that block
     * @return nothing or error; if error happens, no changes in the tree are
     * made
     */
    virtual outcome::result<void> addExistingBlock(
        const BlockHash &block_hash, const BlockHeader &block_header) = 0;

    /**
     * Add a new block to the tree
     * @param block to be stored and added to tree
     * @return nothing or error; if error happens, no changes in the tree are
     * made
     *
     * @note if block, which is specified in PARENT_HASH field of (\param block)
     * is not in our local storage, corresponding error is returned. It is
     * suggested that after getting that error, the caller would ask another
     * peer for the parent block and try to insert it; this operation is to be
     * repeated until a successful insertion happens
     */
    virtual outcome::result<void> addBlock(
        SignedBlockWithAttestation signed_block_with_attestation) = 0;

    /**
     * Remove leaf
     * @param block_hash - hash of block to be deleted. The block must be leaf.
     * @return nothing or error
     */
    virtual outcome::result<void> removeLeaf(const BlockHash &block_hash) = 0;

    /**
     * Mark the block as finalized and store a finalization justification
     * @param block to be finalized
     * @param justification of the finalization
     * @return nothing or error
     */
    virtual outcome::result<void> finalize(
        const BlockHash &block, const Justification &justification) = 0;

    /**
     * Get a chain of blocks from provided block to direction of the best block
     * @param block from which the chain is started
     * @param maximum number of blocks to be retrieved
     * @return chain or blocks or error
     */
    [[nodiscard]] virtual outcome::result<std::vector<BlockHash>>
    getBestChainFromBlock(const BlockHash &block, uint64_t maximum) const = 0;

    /**
     * Get a chain of blocks before provided block including its
     * @param block to which the chain is ended
     * @param maximum number of blocks to be retrieved
     * @return chain or blocks or error
     */
    [[nodiscard]] virtual outcome::result<std::vector<BlockHash>>
    getDescendingChainToBlock(const BlockHash &block,
                              uint64_t maximum) const = 0;

    [[nodiscard]] virtual bool isFinalized(const BlockIndex &block) const = 0;

    /**
     * Get a best leaf of the tree
     * @return best leaf
     *
     * @note best block is also a result of "SelectBestChain": if we are the
     * leader, we connect a block, which we constructed, to that best block
     */
    [[nodiscard]] virtual BlockIndex bestBlock() const = 0;

    /**
     * @brief Get the most recent block of the best (longest) chain among
     * those that contain a block with \param target_hash
     * @param target_hash is a hash of a block that the chosen chain must
     * contain
     */
    [[nodiscard]] virtual outcome::result<BlockIndex> getBestContaining(
        const BlockHash &target_hash) const = 0;

    /**
     * Get all leaves of our tree
     * @return collection of the leaves
     */
    [[nodiscard]] virtual std::vector<BlockHash> getLeaves() const = 0;

    /**
     * Get children of the block with specified hash
     * @param block to get children of
     * @return collection of children hashes or error
     */
    [[nodiscard]] virtual outcome::result<std::vector<BlockHash>> getChildren(
        const BlockHash &block) const = 0;

    /**
     * Get the last finalized block
     * @return hash of the block
     */
    [[nodiscard]] virtual BlockIndex lastFinalized() const = 0;

    /**
     * Get the latest justified checkpoint
     * @return checkpoint
     */
    [[nodiscard]] virtual Checkpoint getLatestJustified() const = 0;

    /**
     * Get `SignedBlockWithAttestation` for
     * "/leanconsensus/req/blocks_by_root/1/ssz_snappy" protocol.
     */
    virtual outcome::result<std::optional<SignedBlockWithAttestation>>
    tryGetSignedBlock(const BlockHash block_hash) const = 0;

    // TODO(turuslan): state transition function
    /**
     * Import pre-sorted batch of `SignedBlockWithAttestation`.
     * May change best and finalized block.
     */
    virtual void import(std::vector<SignedBlockWithAttestation> blocks) = 0;
  };

}  // namespace lean::blockchain
