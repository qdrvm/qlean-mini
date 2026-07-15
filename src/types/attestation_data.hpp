/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "serde/json_fwd.hpp"
#include "types/checkpoint.hpp"
#include "types/slot.hpp"

namespace lean {
  struct AttestationData : ssz::ssz_container {
    Slot slot;
    Checkpoint head;
    Checkpoint target;
    Checkpoint source;

    SSZ_AND_JSON_FIELDS(slot, head, target, source);
    bool operator==(const AttestationData &) const = default;

    /**
     * Check that every checkpoint points to a block on the given chain.
     */
    bool liesOnChain(const std::vector<BlockHash> &history) const {
      // Reject zero-hash checkpoints up front.
      // Empty slots carry the zero hash on the chain.
      // A vote whose recorded root equals the zero hash is meaningless.
      if (source.root == kZeroHash or target.root == kZeroHash
          or head.root == kZeroHash) {
        return false;
      }

      // Reject checkpoints whose slot is beyond the chain view.
      // Without this guard, indexed access raises IndexError.
      if (source.slot >= history.size() or target.slot >= history.size()
          or head.slot >= history.size()) {
        return false;
      }

      // All checkpoint roots must match the chain at their slot.
      return source.root == history.at(source.slot)
         and target.root == history.at(target.slot)
         and head.root == history.at(head.slot);
    }
  };
}  // namespace lean
