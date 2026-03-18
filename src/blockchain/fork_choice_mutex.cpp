/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/fork_choice_mutex.hpp"

#include "blockchain/fork_choice.hpp"
#include "types/fork_choice_api_json.hpp"

namespace lean {
  ForkChoiceStoreMutex::ForkChoiceStoreMutex(
      qtils::SharedRef<ForkChoiceStore> fork_choice)
      : fork_choice_{std::move(fork_choice)} {}

  Checkpoint ForkChoiceStoreMutex::getLatestFinalized() const {
    std::shared_lock lock{mutex_};
    return fork_choice_->getLatestFinalized();
  }

  Checkpoint ForkChoiceStoreMutex::getLatestJustified() const {
    std::shared_lock lock{mutex_};
    return fork_choice_->getLatestJustified();
  }

  outcome::result<std::shared_ptr<const State>> ForkChoiceStoreMutex::getState(
      const BlockHash &block_hash) const {
    std::unique_lock lock{mutex_};
    return fork_choice_->getState(block_hash);
  }

  outcome::result<void> ForkChoiceStoreMutex::onGossipAttestation(
      const SignedAttestation &signed_attestation) {
    std::unique_lock lock{mutex_};
    return fork_choice_->onGossipAttestation(signed_attestation);
  }

  outcome::result<void> ForkChoiceStoreMutex::onGossipAggregatedAttestation(
      const SignedAggregatedAttestation &signed_aggregated_attestation) {
    std::unique_lock lock{mutex_};
    return fork_choice_->onGossipAggregatedAttestation(
        signed_aggregated_attestation);
  }

  outcome::result<void> ForkChoiceStoreMutex::onBlock(
      SignedBlockWithAttestation signed_block_with_attestation) {
    std::unique_lock lock{mutex_};
    return fork_choice_->onBlock(std::move(signed_block_with_attestation));
  }

  std::vector<ForkChoiceStoreMutex::OnTickAction> ForkChoiceStoreMutex::onTick(
      std::chrono::milliseconds now) {
    std::unique_lock lock{mutex_};
    return fork_choice_->onTick(now);
  }

  outcome::result<ForkChoiceApiJson> ForkChoiceStoreMutex::apiForkChoice()
      const {
    std::shared_lock lock{mutex_};
    return fork_choice_->apiForkChoice();
  }

}  // namespace lean
