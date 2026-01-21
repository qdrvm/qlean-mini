/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/block_body.hpp"
#include "types/block_header.hpp"

namespace lean {
  struct Block : ssz::ssz_variable_size_container {
    uint64_t slot;
    uint64_t proposer_index;
    qtils::ByteArr<32> parent_root;
    qtils::ByteArr<32> state_root;
    BlockBody body;

    SSZ_CONT(slot, proposer_index, parent_root, state_root, body);
    bool operator==(const Block &) const = default;

    BlockHeader getHeader() const {
      BlockHeader header;
      header.slot = slot;
      header.proposer_index = proposer_index;
      header.parent_root = parent_root;
      header.state_root = state_root;
      header.body_root = sszHash(body);
      return header;
    }

    mutable std::optional<BlockHash> hash_cached;
    const BlockHash &hash() const {
      return hash_cached.has_value() ? hash_cached.value()
                                     : (setHash(), hash_cached.value());
    }
    void setHash() const {
      auto header = getHeader();
      header.updateHash();
      auto hash = header.hash();
      BOOST_ASSERT(not hash_cached.has_value() or hash == hash_cached);
      hash_cached = hash;
    }

    BlockIndex index() const {
      return {slot, hash()};
    }
  };

  struct AnchorBlock : Block {
    using Block::Block;
    virtual ~AnchorBlock() = 0;
  };
  inline AnchorBlock::~AnchorBlock() = default;
}  // namespace lean
