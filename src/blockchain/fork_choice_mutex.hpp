/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <shared_mutex>

#include <qtils/shared_ref.hpp>

#include "types/block_hash.hpp"
#include "types/signed_aggregated_attestation.hpp"
#include "types/signed_attestation.hpp"
#include "types/signed_block_with_attestation.hpp"

namespace lean {
  class ForkChoiceStore;
  struct Checkpoint;
  struct ForkChoiceApiJson;
  struct State;

  /**
   * Protect public ForkChoiceStore methods called from different threads.
   * Keep internal ForkChoiceStore methods public for tests.
   */
  class ForkChoiceStoreMutex {
   public:
    ForkChoiceStoreMutex(qtils::SharedRef<ForkChoiceStore> fork_choice);

    Checkpoint getLatestFinalized() const;
    Checkpoint getLatestJustified() const;
    outcome::result<std::shared_ptr<const State>> getState(
        const BlockHash &block_hash) const;
    outcome::result<void> onGossipAttestation(
        const SignedAttestation &signed_attestation);
    outcome::result<void> onGossipAggregatedAttestation(
        const SignedAggregatedAttestation &signed_aggregated_attestation);
    outcome::result<void> onBlock(
        SignedBlockWithAttestation signed_block_with_attestation);

    using OnTickAction = std::variant<SignedAttestation,
                                      SignedAggregatedAttestation,
                                      SignedBlockWithAttestation>;
    std::vector<OnTickAction> onTick(std::chrono::milliseconds now);
    outcome::result<ForkChoiceApiJson> apiForkChoice() const;

   private:
    qtils::SharedRef<ForkChoiceStore> fork_choice_;
    mutable std::shared_mutex mutex_;
  };
}  // namespace lean
