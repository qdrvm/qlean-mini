/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/block_body.hpp"
#include "types/block_header.hpp"

namespace lean {
  struct Block : ssz::ssz_container {
    uint64_t slot;
    uint64_t proposer_index;
    qtils::ByteArr<32> parent_root;
    qtils::ByteArr<32> state_root;
    BlockBody body;

    SSZ_CONT(slot, proposer_index, parent_root, state_root, body);

    BlockHeader getHeader() const {
      BlockHeader header;
      header.slot = slot;
      header.proposer_index = proposer_index;
      header.parent_root = parent_root;
      header.state_root = state_root;
      header.body_root = sszHash(body);
      return header;
    }

    std::optional<BlockHash> hash_cached;
    const BlockHash &hash() const {
      return hash_cached.value();
    }
    void setHash() {
      BOOST_ASSERT(not hash_cached.has_value());
      auto header = getHeader();
      header.updateHash();
      hash_cached = header.hash();
    }

    BlockIndex slotHash() const {
      return {slot, hash()};
    }
  };
}  // namespace lean
