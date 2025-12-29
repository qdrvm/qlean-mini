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

#include "app/validator_keys_manifest.hpp"
#include "blockchain/is_justifiable_slot.hpp"
#include "blockchain/state_transition_function.hpp"
#include "blockchain/validator_registry.hpp"
#include "clock/clock.hpp"
#include "crypto/xmss/xmss_provider.hpp"
#include "types/block.hpp"
#include "types/signed_attestation.hpp"
#include "types/signed_block_with_attestation.hpp"
#include "types/state.hpp"
#include "types/validator_index.hpp"
#include "utils/ceil_div.hpp"
#include "utils/lru_cache.hpp"

namespace lean {
  struct GenesisConfig;
}  // namespace lean

namespace lean::blockchain {
  class BlockStorage;
  class BlockTree;
}

namespace lean::metrics {
  class Metrics;
}

namespace lean {
  /**
   * Forkchoice store tracking chain state and validator attestations.
   *
   * This is the "local view" that a node uses to run LMD GHOST. It contains:
   *
   * - which blocks and states are known,
   * - which checkpoints are justified and finalized,
   * - which block is currently considered the head,
   * - and, for each validator, their latest attestation that should influence
   *   fork choice.
   *
   * The Store is updated whenever:
   * - a new block is processed,
   * - an attestation is received (via a block or gossip),
   * - an interval tick occurs (activating new attestations),
   * - or when the head is recomputed.
   */
  class ForkChoiceStore {
   public:
    //    using Blocks = std::unordered_map<BlockHash,
    //    SignedBlockWithAttestation>;
    using SignedAttestations =
        std::unordered_map<ValidatorIndex, SignedAttestation>;

    enum class Error {
      INVALID_ATTESTATION,
      INVALID_PROPOSER,
      STATE_NOT_FOUND,
    };
    Q_ENUM_ERROR_CODE_FRIEND(Error) {
      using E = decltype(e);
      switch (e) {
        case E::INVALID_ATTESTATION:
          return "Invalid attestation";
        case E::INVALID_PROPOSER:
          return "Invalid proposer";
        case E::STATE_NOT_FOUND:
          return "Parent state not found";
      }
      abort();
    }

    ForkChoiceStore(
        qtils::SharedRef<AnchorState> anchor_state,
        qtils::SharedRef<AnchorBlock> anchor_block,
        qtils::SharedRef<clock::SystemClock> clock,
        qtils::SharedRef<log::LoggingSystem> logging_system,
        qtils::SharedRef<metrics::Metrics> metrics,
        qtils::SharedRef<ValidatorRegistry> validator_registry,
        qtils::SharedRef<app::ValidatorKeysManifest> validator_keys_manifest,
        qtils::SharedRef<crypto::xmss::XmssProvider> xmss_provider,
        qtils::SharedRef<blockchain::BlockTree> block_tree,
        qtils::SharedRef<blockchain::BlockStorage> block_storage);

    BOOST_DI_INJECT_TRAITS(qtils::SharedRef<AnchorState>,
                           qtils::SharedRef<AnchorBlock>,
                           qtils::SharedRef<clock::SystemClock>,
                           qtils::SharedRef<log::LoggingSystem>,
                           qtils::SharedRef<metrics::Metrics>,
                           qtils::SharedRef<ValidatorRegistry>,
                           qtils::SharedRef<app::ValidatorKeysManifest>,
                           qtils::SharedRef<crypto::xmss::XmssProvider>,
                           qtils::SharedRef<blockchain::BlockTree>,
                           qtils::SharedRef<blockchain::BlockStorage>);

    // Test constructor - only for use in tests
    ForkChoiceStore(
        uint64_t now_sec,
        qtils::SharedRef<log::LoggingSystem> logging_system,
        qtils::SharedRef<metrics::Metrics> metrics,
        Config config,
        BlockHash head,
        BlockHash safe_target,
        Checkpoint latest_justified,
        Checkpoint latest_finalized,
        // Blocks blocks,
        // std::unordered_map<BlockHash, State> states,
        SignedAttestations latest_known_attestations,
        SignedAttestations latest_new_votes,
        ValidatorIndex validator_index,
        qtils::SharedRef<ValidatorRegistry> validator_registry,
        qtils::SharedRef<app::ValidatorKeysManifest> validator_keys_manifest,
        qtils::SharedRef<crypto::xmss::XmssProvider> xmss_provider,
        qtils::SharedRef<blockchain::BlockTree> block_tree,
        qtils::SharedRef<blockchain::BlockStorage> block_storage);

    // Compute the latest block that the validator is allowed to choose as the
    // target
    outcome::result<void> updateSafeTarget();

    // Updates the store's latest justified checkpoint, head, and latest
    // finalized state.
    void updateHead();

    // Process new attestations that the staker has received. Attestations
    // processing is done at a particular time, because of safe target and view
    // merge rules. Accepts the latest new votes, merges them into the known
    // votes, and then updates the fork-choice head.
    void acceptNewAttestations();

    Slot getCurrentSlot() const;

    BlockHash getHead();
    outcome::result<std::shared_ptr<const State>> getState(
        const BlockHash &block_hash) const;

    bool hasBlock(const BlockHash &hash) const;
    std::optional<Slot> getBlockSlot(const BlockHash &block_hash) const;
    Slot getHeadSlot() const;
    const Config &getConfig() const;
    Checkpoint getLatestFinalized() const;
    Checkpoint getLatestJustified() const;

    // Test helper methods
    BlockHash getSafeTarget() const {
      return safe_target_;
    }
    // const Blocks &getBlocks() const {
    //   return blocks_;
    // }
    const SignedAttestations &getLatestNewAttestations() const {
      return latest_new_attestations_;
    }
    const SignedAttestations &getLatestKnownAttestations() const {
      return latest_known_attestations_;
    }
    SignedAttestations &getLatestNewVotesRef() {
      return latest_new_attestations_;
    }

    /**
     * Internal implementation of LMD GHOST fork choice algorithm.
     *
     * Walk the block tree according to the LMD GHOST rule.
     *
     * The walk starts from a chosen root.
     * At each fork, the child subtree with the highest weight is taken.
     * The process stops when a leaf is reached.
     * That leaf is the chosen head.
     *
     * Weights are derived from votes as follows:
     * - Each validator contributes its full weight to its most recent head
     * vote.
     * - The weight of that vote also flows to every ancestor of the voted
     * block.
     * - The weight of a subtree is the sum of all such contributions inside
     * it.
     *
     * An optional threshold can be applied:
     * - If a threshold is set, children below this threshold are ignored.
     *
     * When two branches have equal weight, the one with the lexicographically
     * larger hash is chosen to break ties.
     *
     * Args:
     *     start_root: Starting point root (usually latest justified).
     *     attestations: Attestations to consider for fork choice weights.
     *     min_score: Minimum attestation count for block inclusion.
     *
     * Returns:
     *     Hash of the chosen head block.
     */
    BlockHash computeLmdGhostHead(const BlockHash &start_root,
                                  const SignedAttestations &attestations,
                                  uint64_t min_score = 0) const;

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
     *  2. **Walk Toward Safe**: Move backward (up to
     * `JUSTIFICATION_LOOKBACK_SLOTS` steps) if safe target is newer
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
     * Produce the attestation data for a validator at the given slot.
     *
     * This helper constructs the attestation data payload that describes the
     * validator's view of the chain (head, target, source) for the requested
     * slot. The caller can reuse the result to sign or broadcast an
     * attestation.
     */
    AttestationData produceAttestationData(Slot slot) const;

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
    Attestation produceAttestation(Slot slot,
                                   ValidatorIndex validator_index) const;

    /**
     * Validate incoming attestation before processing.
     *
     * Ensures the vote respects the basic laws of time and topology:
     *     1. The blocks voted for must exist in our store.
     *     2. A vote cannot span backwards in time (source > target).
     *     3. A vote cannot be for a future slot.
     *
     * Args:
     *     signed_attestation: Attestation to validate.
     *
     * Returns:
     *     Success if validation passes, error otherwise.
     */
    outcome::result<void> validateAttestation(
        const SignedAttestation &signed_attestation);

    /**
     * Process a new attestation and place it into the correct attestation
     * stage.
     *
     * Attestations can come from:
     * - a block body (on-chain, is_from_block=true), or
     * - the gossip network (off-chain, is_from_block=false).
     *
     * The Attestation Pipeline
     * -------------------------
     * Attestations always live in exactly one of two dictionaries:
     *
     * Stage 1: latest new attestations
     *     - Holds *pending* attestations that are not yet counted in fork
     *       choice.
     *     - Includes the proposer's attestation for the block they just
     *       produced.
     *     - Await activation by an interval tick before they influence weights.
     *
     * Stage 2: latest known attestations
     *     - Contains all *active* attestations used by LMD-GHOST.
     *     - Updated during interval ticks, which promote new → known.
     *     - Directly contributes to fork-choice subtree weights.
     *
     * Key Behaviors
     * --------------
     * Migration:
     *     - Attestations always move forward (new → known), never backwards.
     *
     * Superseding:
     *     - For each validator, only the attestation from the highest slot is
     *       kept.
     *     - A newer attestation overwrites an older one in either dictionary.
     *
     * Accumulation:
     *     - Attestations from different validators accumulate independently.
     *     - Only same-validator comparisons result in replacement.
     *
     * Args:
     *     signed_attestation: The attestation message to ingest.
     *     is_from_block: True if embedded in a block body (on-chain),
     *                    False if from gossip.
     *
     * Returns:
     *     Success if the attestation was processed, error otherwise.
     */
    outcome::result<void> onAttestation(
        const SignedAttestation &signed_attestation, bool is_from_block);


    // Processes a new block, updates the store, and triggers a head update.
    outcome::result<void> onBlock(
        SignedBlockWithAttestation signed_block_with_attestation);

    // Advance forkchoice store time to given timestamp.
    // Ticks store forward interval by interval, performing appropriate
    // actions for each interval type.
    // Args:
    //    time: Target time in seconds since genesis.
    std::vector<std::variant<SignedAttestation, SignedBlockWithAttestation>>
    onTick(uint64_t now_sec);

    Interval time() const {
      return time_;
    }

   private:
    // Verify all XMSS signatures in a signed block.
    //
    // This method ensures that every attestation included in the block
    // (both on-chain attestations from the block body and the proposer's
    // own attestation) is properly signed by the claimed validator using
    // their registered XMSS public key.
    //
    // Args:
    //     signed_block: Complete signed block containing:
    //         - Block body with included attestations
    //         - Proposer's attestation for this block
    //         - XMSS signatures for all attestations (ordered)
    //
    // Returns:
    //     True if all signatures are cryptographically valid.
    bool validateBlockSignatures(
        const SignedBlockWithAttestation &signed_block) const;

    void updateLastFinalized(const Checkpoint &checkpoint);

    log::Logger logger_;
    qtils::SharedRef<metrics::Metrics> metrics_;
    qtils::SharedRef<crypto::xmss::XmssProvider> xmss_provider_;
    qtils::SharedRef<blockchain::BlockTree> block_tree_;
    qtils::SharedRef<blockchain::BlockStorage> block_storage_;

    STF stf_;
    Interval time_;

    /// Chain configuration parameters.
    Config config_;

    /**
     * Root of the current canonical chain head block.
     *
     * This is the result of running the fork choice algorithm on the current
     * contents of the Store.
     */
    BlockHash head_;

    /**
     * Root of the current safe target for attestation.
     *
     * This can be used by higher-level logic to restrict which blocks are
     * considered safe to attest to, based on additional safety conditions.
     */
    BlockHash safe_target_;

    /**
     * Highest slot justified checkpoint known to the store.
     *
     * LMD GHOST starts from this checkpoint when computing the head.
     *
     * Only descendants of this checkpoint are considered viable.
     */
    Checkpoint latest_justified_;

    mutable LruCache<BlockHash, State> states_{8};

    /**
     * Active attestations that contribute to fork choice weights.
     *
     * Stage 2 of the attestation pipeline:
     * - Contains all *active* attestations used by LMD-GHOST.
     * - Updated during interval ticks, which promote new → known.
     * - Directly contributes to fork-choice subtree weights.
     *
     * For each validator, stores their most recent attestation that is
     * currently influencing the fork choice head computation.
     */
    SignedAttestations latest_known_attestations_;

    /**
     * Pending attestations awaiting activation.
     *
     * Stage 1 of the attestation pipeline:
     * - Holds *pending* attestations that are not yet counted in fork choice.
     * - Includes the proposer's attestation for the block they just produced.
     * - Await activation by an interval tick before they influence weights.
     *
     * Attestations move from this map to latest_known_attestations_ during
     * interval ticks.
     */
    SignedAttestations latest_new_attestations_;
    qtils::SharedRef<ValidatorRegistry> validator_registry_;
    qtils::SharedRef<app::ValidatorKeysManifest> validator_keys_manifest_;
  };

}  // namespace lean
