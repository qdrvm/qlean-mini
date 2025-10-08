/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <optional>
#include <unordered_map>

#include <boost/assert.hpp>
#include <boost/di.hpp>
#include <qtils/shared_ref.hpp>

#include "blockchain/is_justifiable_slot.hpp"
#include "blockchain/state_transition_function.hpp"
#include "clock/clock.hpp"
#include "types/block.hpp"
#include "types/state.hpp"
#include "types/validator_index.hpp"
#include "utils/ceil_div.hpp"
#include "utils/validator_registry.hpp"

namespace lean {
  class ForkChoiceStore {
   public:
    using Blocks = std::unordered_map<BlockHash, Block>;
    using Votes = std::unordered_map<ValidatorIndex, SignedVote>;

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

    ForkChoiceStore(const AnchorState &anchor_state,
                    const AnchorBlock &anchor_block,
                    qtils::SharedRef<clock::SystemClock> clock,
                    qtils::SharedRef<log::LoggingSystem> logging_system,
                    qtils::SharedRef<ValidatorRegistry> validator_registry);

    BOOST_DI_INJECT_TRAITS(const AnchorState &,
                           const AnchorBlock &,
                           qtils::SharedRef<clock::SystemClock>,
                           qtils::SharedRef<log::LoggingSystem>,
                           qtils::SharedRef<ValidatorRegistry>);
    // Test constructor - only for use in tests
    ForkChoiceStore(
        uint64_t now_sec,
        qtils::SharedRef<log::LoggingSystem> logging_system,
        Config config = {},
        BlockHash head = {},
        BlockHash safe_target = {},
        Checkpoint latest_justified = {},
        Checkpoint latest_finalized = {},
        Blocks blocks = {},
        std::unordered_map<BlockHash, State> states = {},
        Votes latest_known_votes = {},
        Votes latest_new_votes = {},
        ValidatorIndex validator_index = 0,
        std::shared_ptr<ValidatorRegistry> validator_registry = nullptr);

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

    Slot getCurrentSlot();

    BlockHash getHead();
    const State &getState(const BlockHash &block_hash) const;

    bool hasBlock(const BlockHash &hash) const;
    std::optional<Slot> getBlockSlot(const BlockHash &block_hash) const;
    Slot getHeadSlot() const;
    const Config &getConfig() const;
    Checkpoint getLatestFinalized() const;

    // Test helper methods
    BlockHash getSafeTarget() const {
      return safe_target_;
    }
    const Blocks &getBlocks() const {
      return blocks_;
    }
    const Votes &getLatestNewVotes() const {
      return latest_new_votes_;
    }
    const Votes &getLatestKnownVotes() const {
      return latest_known_votes_;
    }
    Votes &getLatestNewVotesRef() {
      return latest_new_votes_;
    }

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
    outcome::result<Block> produceBlock(Slot slot,
                                        ValidatorIndex validator_index);

    // Validate incoming attestation before processing.
    // Performs basic validation checks on attestation structure and timing.
    outcome::result<void> validateAttestation(const SignedVote &signed_vote);

    // Validates and processes a new attestation (a signed vote), updating the
    // store's latest votes.
    outcome::result<void> processAttestation(const SignedVote &signed_vote,
                                             bool is_from_block);

    // Processes a new block, updates the store, and triggers a head update.
    outcome::result<void> onBlock(Block block);

    // Advance forkchoice store time to given timestamp.
    // Ticks store forward interval by interval, performing appropriate
    // actions for each interval type.
    // Args:
    //    time: Target time in seconds since genesis.
    //    has_proposal: Whether node has proposal for current slot.
    std::vector<std::variant<SignedVote, SignedBlock>> advanceTime(
        uint64_t now_sec);

   private:
    STF stf_;
    Interval time_;
    Config config_;
    BlockHash head_;
    BlockHash safe_target_;
    Checkpoint latest_justified_;
    Checkpoint latest_finalized_;
    Blocks blocks_;
    std::unordered_map<BlockHash, State> states_;
    Votes latest_known_votes_;
    Votes latest_new_votes_;
    std::shared_ptr<ValidatorRegistry> validator_registry_;
    log::Logger logger_;
  };

  BlockHash getForkChoiceHead(const ForkChoiceStore::Blocks &blocks,
                              const Checkpoint &root,
                              const ForkChoiceStore::Votes &latest_votes,
                              uint64_t min_score);
}  // namespace lean
