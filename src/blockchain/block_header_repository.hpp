/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <optional>

#include "types/block_header.hpp"

namespace lean::blockchain {

  /**
   * Status of a block
   */
  enum class BlockStatus : uint8_t {
    InChain,
    Unknown,
  };

  /**
   * An interface to a storage with block headers that provides several
   * convenience methods, such as getting bloch number by its hash and vice
   * versa or getting a block status
   */
  class BlockHeaderRepository {
   public:
    virtual ~BlockHeaderRepository() = default;

    /**
     * @return the number of the block with the provided {@param block_hash}
     * in case one is in the storage or an error
     */
    [[nodiscard]] virtual outcome::result<Slot> getNumberByHash(
        const BlockHash &block_hash) const = 0;

    /**
     * @return block header with corresponding {@param block_hash} or an error
     */
    [[nodiscard]] virtual outcome::result<BlockHeader> getBlockHeader(
        const BlockHash &block_hash) const = 0;

    /**
     * @return block header with corresponding {@param block_hash} or a none
     * optional if the corresponding block header is not in storage or a
     * storage error
     */
    [[nodiscard]] virtual outcome::result<std::optional<BlockHeader>>
    tryGetBlockHeader(const BlockHash &block_hash) const = 0;
  };

}  // namespace lean::blockchain
