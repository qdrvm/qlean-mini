/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <optional>
#include <unordered_map>

#include <boost/assert.hpp>

#include "blockchain/is_justifiable_slot.hpp"
#include "blockchain/state_transition_function.hpp"
#include "types/block.hpp"
#include "types/state.hpp"
#include "types/validator_index.hpp"
#include "utils/ceil_div.hpp"

namespace lean {
  class ForkChoiceStore {
   public:
    using Blocks = std::unordered_map<BlockHash, Block>;
    using Votes = std::unordered_map<ValidatorIndex, Checkpoint>;

    enum class Error {
      INVALID_ATTESTATION,
      INVALID_PROPOSER,
    };
    Q_ENUM_ERROR_CODE_FRIEND(Error) {
      using E = decltype(e);
      switch (e) {
        case E::INVALID_ATTESTATION:
          return "Invalid attestation";
        case E::INVALID_PROPOSER:
          return "Invalid proposer";
      }
      abort();
    }

    ForkChoiceStore(State anchor_state, Block anchor_block);

    // Compute the latest block that the validator is allowed to choose as the
    // target
    void updateSafeTarget();

    std::optional<Checkpoint> getLatestJustified();

    // Updates the store's latest justified checkpoint, head, and latest
    // finalized state.
    void updateHead();

    // Process new votes that the staker has received. Vote processing is done
    // at a particular time, because of safe target and view merge rules.
    // Accepts the latest new votes, merges them into the known votes, and then
    // updates the fork-choice head.
    void acceptNewVotes();

    void tickInterval(bool has_proposal);

    // called every interval and with has_proposal flag on the new slot interval
    // if node has a validator with proposal in this slot so as to not delay
    // accepting new votes and parallelize compute.
    // Ticks the store forward in intervals until it reaches the given time.
    void advanceTime(Interval time, bool has_proposal);

    Slot getCurrentSlot();

    BlockHash getHead();
    State getState(const BlockHash &block_hash) const;

    bool hasBlock(const BlockHash &hash) const;
    Slot getBlockSlot(const BlockHash &block_hash) const;
    Slot getHeadSlot() const;
    const Config &getConfig() const;
    Checkpoint getLatestFinalized() const;

    /**
     * Calculates the target checkpoint for a vote based on the head, safe
     * target, and latest finalized state.
     */
    Checkpoint getVoteTarget() const;

    /**
     * Produce a new block for the given slot and validator.
     *
     * Algorithm Overview:
     * 1. Validate proposer authorization for the target slot
     * 2. Get the current chain head as the parent block
     * 3. Iteratively build attestation set:
     *    - Create candidate block with current attestations
     *    - Apply state transition (slot advancement + block processing)
     *    - Find new valid attestations matching post-state requirements
     *    - Continue until no new attestations can be added
     * 4. Finalize block with computed state root and store it
     *
     * Args:
     *   slot: Target slot number for block production
     *   validator_index: Index of validator authorized to propose this block
     */
    outcome::result<Block> produceBlock(Slot slot, ValidatorIndex validator_index);

    // Validate incoming attestation before processing.
    // Performs basic validation checks on attestation structure and timing.
    outcome::result<void> validateAttestation(const SignedVote &signed_vote);

    // Validates and processes a new attestation (a signed vote), updating the
    // store's latest votes.
    outcome::result<void> processAttestation(const SignedVote &signed_vote,
                                             bool is_from_block);

    // Processes a new block, updates the store, and triggers a head update.
    outcome::result<void> onBlock(Block block);

   private:
    STF stf_;
    Interval time_ = 0;
    Config config_;
    BlockHash head_;
    BlockHash safe_target_;
    Checkpoint latest_justified_;
    Checkpoint latest_finalized_;
    Blocks blocks_;
    std::unordered_map<BlockHash, State> states_;
    Votes latest_known_votes_;
    std::unordered_map<ValidatorIndex, SignedVote> signed_votes_;
    Votes latest_new_votes_;
  };

  BlockHash getForkChoiceHead(const ForkChoiceStore::Blocks &blocks,
                              const Checkpoint &root,
                              const ForkChoiceStore::Votes &latest_votes,
                              uint64_t min_score);
}  // namespace lean
