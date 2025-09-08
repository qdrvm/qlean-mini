/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <types/types.hpp>
#include <utils/custom_equality.hpp>

#include "serde/serialization.hpp"
#include "types/block_index.hpp"

namespace lean {

  /**
   * @struct BlockHeader
   * This is a lighter version of the block, used for referencing and
   * verification.
   */
  class BlockHeader : public ssz::ssz_container {
   public:
    /// The block’s slot number
    Slot slot;
    /// Index of the validator that proposed the block
    ProposerIndex proposer_index;
    /// Hash of the parent block
    HeaderHash parent_root;
    /// Hash of the post-state after the block is processed
    StateRoot state_root;
    /// The block’s body, containing further operations and data
    BodyRoot body_root;

    BlockHeader() = default;

    /// Block hash if calculated
    mutable std::optional<HeaderHash> hash_opt{};

    CUSTOM_EQUALITY(
        BlockHeader, slot, proposer_index, parent_root, state_root, body_root);

    SSZ_CONT(slot, proposer_index, parent_root, state_root, body_root);

    const HeaderHash &hash() const {
      BOOST_ASSERT_MSG(hash_opt.has_value(),
                       "Hash must be calculated and saved before that");
      return hash_opt.value();
    }

    void updateHash() const {
      hash_opt = sszHash(*this);
    }

    BlockIndex index() const {
      return {slot, hash()};
    }
  };
}  // namespace lean
