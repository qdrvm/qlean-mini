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
#include "blockchain/validator_registry.hpp"
#include "clock/clock.hpp"
#include "types/block.hpp"
#include "types/signed_attestation.hpp"
#include "types/signed_block_with_attestation.hpp"
#include "types/state.hpp"
#include "types/validator_index.hpp"
#include "utils/ceil_div.hpp"

namespace lean {
  struct GenesisConfig;
}  // namespace lean

namespace lean::metrics {
  class Metrics;
}  // namespace lean::metrics

namespace lean {
  class ForkChoiceStore {
   public:
    using Blocks = std::unordered_map<BlockHash, Block>;
    using AttestationMap =
        std::unordered_map<ValidatorIndex, SignedAttestation>;

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

    ForkChoiceStore(const GenesisConfig &genesis_config,
                    qtils::SharedRef<clock::SystemClock> clock,
                    qtils::SharedRef<log::LoggingSystem> logging_system,
                    qtils::SharedRef<metrics::Metrics> metrics,
                    qtils::SharedRef<ValidatorRegistry> validator_registry);

    BOOST_DI_INJECT_TRAITS(const GenesisConfig &,
                           qtils::SharedRef<clock::SystemClock>,
                           qtils::SharedRef<log::LoggingSystem>,
                           qtils::SharedRef<metrics::Metrics>,
                           qtils::SharedRef<ValidatorRegistry>);
    // Test constructor - only for use in tests
    ForkChoiceStore(uint64_t now_sec,
                    qtils::SharedRef<log::LoggingSystem> logging_system,
                    qtils::SharedRef<metrics::Metrics> metrics,
                    Config config,
                    BlockHash head,
                    BlockHash safe_target,
                    Checkpoint latest_justified,
                    Checkpoint latest_finalized,
                    Blocks blocks,
                    std::unordered_map<BlockHash, State> states,
                    AttestationMap latest_known_attestations,
                    AttestationMap latest_new_attestations,
                    ValidatorIndex validator_index,
                    qtils::SharedRef<ValidatorRegistry> validator_registry);

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
    void acceptNewAttestations();

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
    const AttestationMap &getLatestNewAttestations() const {
      return latest_new_attestations_;
    }
    const AttestationMap &getLatestKnownAttestations() const {
      return latest_known_attestations_;
    }

    /**
     * Calculate target checkpoint for validator attestations.
     *
     *  Determines appropriate attestation target based on head, safe target,
     *  and finalization constraints. The target selection algorithm balances
     *  between advancing the chain head and maintaining safety guarantees.
     *
     *  Attestation Target Algorithm
     *  -----------------------------
     *  The algorithm walks back from the current head toward the safe target,
     *  ensuring the target is in a justifiable slot range:
     *
     *  1. **Start at Head**: Begin with the current head block
     *  2. **Walk Toward Safe**: Move backward (up to 3 steps) if safe target is
     * newer
     *  3. **Ensure Justifiable**: Continue walking back until slot is
     * justifiable
     *  4. **Return Checkpoint**: Create checkpoint from selected block
     *
     *  Justifiability Rules (see Slot.is_justifiable_after)
     *  ------------------------------------------------------
     *  A slot is justifiable at distance delta from finalization if:
     *  1. delta ≤ 5 (first 5 slots always justifiable)
     *  2. delta is a perfect square (1, 4, 9, 16, 25, ...)
     *  3. delta is a pronic number (2, 6, 12, 20, 30, ...)
     *
     *  These rules prevent long-range attacks by restricting which checkpoints
     *  validators can attest to relative to finalization.
     *
     *  Returns:
     *      Target checkpoint for attestation.
     */
    Checkpoint getAttestationTarget() const;

    /**
     * Produce a block and attestation signatures for the target slot.
     *
     *  The proposer returns the block and a naive signature list so it can
     *  later craft its `SignedBlockWithAttestation` with minimal extra work.
     *
     *  Algorithm Overview:
     *  1. Validate proposer authorization for the target slot
     *  2. Get the current chain head as the parent block
     *  3. Iteratively build attestation set:
     *     - Create candidate block with current attestations
     *     - Apply state transition (slot advancement + block processing)
     *     - Find new valid attestations matching post-state requirements
     *     - Continue until no new attestations can be added
     *  4. Finalize block with computed state root and store it
     *
     *  Args:
     *      slot: Target slot number for block production
     *      validator_index: Index of validator authorized to propose this block
     *
     *  Returns:
     *      Complete block with maximal attestation set and valid state root
     */
    outcome::result<SignedBlockWithAttestation> produceBlockWithSignatures(
        Slot slot, ValidatorIndex validator_index);


    /**
     * Produce an attestation for the given slot and validator.
     *
     * This method constructs an Attestation object according to the lean
     * protocol specification for attestation. The attestation represents the
     * validator's view of the chain state and their choice for the
     * next justified checkpoint.
     *
     * The algorithm:
     * 1. Get the current head
     * 2. Calculate the appropriate attestation target using current forkchoice
     * state
     * 3. Use the store's latest justified checkpoint as the attestation source
     * 4. Construct and return the complete Attestation object
     *
     * Args:
     *     slot: The slot for which to produce the attestation.
     *     validator_index: The validator index producing the attestation.
     *
     * Returns:
     *     A fully constructed Attestation object ready for signing and
     * broadcast.
     */
    Attestation produceAttestation(Slot slot, ValidatorIndex validator_index);

    // Validate incoming attestation before processing.
    // Performs basic validation checks on attestation structure and timing.
    outcome::result<void> validateAttestation(
        const SignedAttestation &signed_attestation);

    // Validates and processes a new attestation (a signed vote), updating the
    // store's latest votes.
    outcome::result<void> onAttestation(
        const SignedAttestation &signed_attestation, bool is_from_block);

    /*
      Process the proposer's attestation for their own block.

      The proposer attestation is handled specially to prevent circular weight:

      Timing
      ------
      - Cast during interval 1 (after block proposal in interval 0)
      - Processed as gossip (`is_from_block=False`) to avoid circular weight
      - Becomes "known" only in interval 3, after fork choice update
      - Will be included in a future block by another proposer

      Why This Matters
      -----------------
      The proposer should not gain unfair fork choice advantage by attesting
      to their own block before it competes with alternatives. This separation
      ensures the proposer's attestation doesn't create circular weight.

      Args:
          proposer_attestation: The proposer's attestation message.
          signatures: Signature list from the signed block.
          block: The block being processed (for index calculation).

      Returns:
          New Store with proposer attestation processed as gossip.
    */
    outcome::result<void> processProposerAttestation(
        SignedBlockWithAttestation signed_block_with_attestation);

    // Processes a new block, updates the store, and triggers a head update.
    outcome::result<void> onBlock(
        SignedBlockWithAttestation signed_block_with_attestation);

    // Advance forkchoice store time to given timestamp.
    // Ticks store forward interval by interval, performing appropriate
    // actions for each interval type.
    // Args:
    //    time: Target time in seconds since genesis.
    //    has_proposal: Whether node has proposal for current slot.
    std::vector<std::variant<SignedAttestation, SignedBlockWithAttestation>>
    advanceTime(uint64_t now_sec);

    Interval time() const {
      return time_;
    }

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
    AttestationMap latest_known_attestations_;
    AttestationMap latest_new_attestations_;
    qtils::SharedRef<ValidatorRegistry> validator_registry_;
    log::Logger logger_;
    qtils::SharedRef<metrics::Metrics> metrics_;
  };

  BlockHash getForkChoiceHead(
      const ForkChoiceStore::Blocks &blocks,
      const Checkpoint &root,
      const ForkChoiceStore::AttestationMap &latest_attestations,
      uint64_t min_score);
}  // namespace lean
