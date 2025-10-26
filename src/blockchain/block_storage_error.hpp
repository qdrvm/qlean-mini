/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>

#include <qtils/enum_error_code.hpp>

namespace lean::blockchain {

  enum class BlockStorageError : uint8_t {
    BLOCK_EXISTS = 1,
    NO_BLOCKS_FOUND,
    HEADER_NOT_FOUND,
    GENESIS_BLOCK_ALREADY_EXISTS,
    GENESIS_BLOCK_NOT_FOUND,
    FINALIZED_BLOCK_NOT_FOUND,
    BLOCK_TREE_LEAVES_NOT_FOUND,
    JUSTIFICATION_EMPTY
  };

}

OUTCOME_HPP_DECLARE_ERROR(lean::blockchain, BlockStorageError);
