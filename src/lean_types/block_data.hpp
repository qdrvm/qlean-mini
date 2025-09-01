/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "block_body.hpp"
#include "block_header.hpp"
#include "block_signature.hpp"
#include "types.hpp"

namespace lean {
  struct BlockData {
    BlockHash hash;
    std::optional<BlockHeader> header;
    std::optional<BlockBody> body;
    std::optional<BlockSignature> signature;
  };
}  // namespace lean
