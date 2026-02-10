/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <optional>
#include <unordered_map>

#include <boost/di.hpp>
#include <qtils/shared_ref.hpp>

#include "blockchain/state_transition_function.hpp"
#include "clock/clock.hpp"
#include "crypto/xmss/xmss_provider.hpp"
#include "injector/boost_di_inject_traits_many.hpp"
#include "log/logger.hpp"
#include "types/aggregated_attestations.hpp"
#include "types/block.hpp"
#include "types/hash.hpp"
#include "types/signed_aggregated_attestation.hpp"
#include "types/signed_attestation.hpp"
#include "types/signed_block_with_attestation.hpp"
#include "types/state.hpp"
#include "types/validator_index.hpp"
#include "utils/lru_cache.hpp"
#include "utils/tuple_hash.hpp"

namespace lean {
  struct GenesisConfig;
  class ValidatorRegistry;
  class ValidatorKeysManifest;
}  // namespace lean

namespace lean::app {
  class ChainSpec;
}  // namespace lean::app

namespace lean::blockchain {
  class BlockStorage;
  class BlockTree;
}

namespace lean::crypto::xmss {
  class XmssProvider;
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
    using AttestationDataByValidator =
        std::unordered_map<ValidatorIndex, AttestationData>;

    enum class Error {
      CANT_VALIDATE_ATTESTATION_SOURCE_NOT_FOUND,
      CANT_VALIDATE_ATTESTATION_TARGET_NOT_FOUND,
      CANT_VALIDATE_ATTESTATION_HEAD_NOT_FOUND,
      INVALID_ATTESTATION,
      INVALID_PROPOSER,
      STATE_NOT_FOUND,
      SIGNATURE_COUNT_MISMATCH,
    };
    Q_ENUM_ERROR_CODE_FRIEND(Error) {
      using E = decltype(e);
      switch (e) {
        case E::CANT_VALIDATE_ATTESTATION_SOURCE_NOT_FOUND:
          return "Can't validate attestation cause source block not found";
        case E::CANT_VALIDATE_ATTESTATION_TARGET_NOT_FOUND:
          return "Can't validate attestation cause target block not found";
        case E::CANT_VALIDATE_ATTESTATION_HEAD_NOT_FOUND:
          return "Can't validate attestation cause head block not found";
        case E::INVALID_ATTESTATION:
          return "Invalid attestation";
        case E::INVALID_PROPOSER:
          return "Invalid proposer";
        case E::STATE_NOT_FOUND:
          return "Parent state not found";
        case E::SIGNATURE_COUNT_MISMATCH:
          return "Signature count must match attestation count";
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
        qtils::SharedRef<app::ChainSpec> chain_spec,
        qtils::SharedRef<app::ValidatorKeysManifest> validator_keys_manifest,
        qtils::SharedRef<crypto::xmss::XmssProvider> xmss_provider,
        qtils::SharedRef<blockchain::BlockTree> block_tree,
        qtils::SharedRef<blockchain::BlockStorage> block_storage);

    BOOST_DI_INJECT_TRAITS_MANY(qtils::SharedRef<AnchorState>,
                                qtils::SharedRef<AnchorBlock>,
                                qtils::SharedRef<clock::SystemClock>,
                                qtils::SharedRef<log::LoggingSystem>,
                                qtils::SharedRef<metrics::Metrics>,
                                qtils::SharedRef<ValidatorRegistry>,
                                qtils::SharedRef<app::ChainSpec>,
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
        Checkpoint head,
        Checkpoint safe_target,
        AttestationDataByValidator latest_known_attestations,
        AttestationDataByValidator latest_new_votes,
        ValidatorIndex validator_index,
        qtils::SharedRef<ValidatorRegistry> validator_registry,
        qtils::SharedRef<app::ValidatorKeysManifest> validator_keys_manifest,
        qtils::SharedRef<crypto::xmss::XmssProvider> xmss_provider,
        qtils::SharedRef<blockchain::BlockTree> block_tree,
        qtils::SharedRef<blockchain::BlockStorage> block_storage,
        bool is_aggregator);

    // Compute the latest block that the validator is allowed to choose as the
    // target
    [[nodiscard]] outcome::result<void> updateSafeTarget();

    // Updates the store's latest justified checkpoint, head, and latest
    // finalized state.
    [[nodiscard]] outcome::result<void> updateHead();

    // Process new attestations that the staker has received. Attestations
    // processing is done at a particular time, because of safe target and view
    // merge rules. Accepts the latest new votes, merges them into the known
    // votes, and then updates the fork-choice head.
    [[nodiscard]] outcome::result<void> acceptNewAttestations();

    Slot getCurrentSlot() const;

    Checkpoint getHead();
    [[nodiscard]] outcome::result<std::shared_ptr<const State>> getState(
        const BlockHash &block_hash) const;

    bool hasBlock(const BlockHash &hash) const;
    [[nodiscard]] outcome::result<Slot> getBlockSlot(
        const BlockHash &block_hash) const;
    const Config &getConfig() const;
    Checkpoint getLatestFinalized() const;
    Checkpoint getLatestJustified() const;

    // Test helper methods
    Checkpoint getSafeTarget() const {
      return safe_target_;
    }
    const AttestationDataByValidator &getLatestNewAttestations() const {
      return latest_new_attestations_;
    }
    const AttestationDataByValidator &getLatestKnownAttestations() const {
      return latest_known_attestations_;
    }
    AttestationDataByValidator &getLatestNewVotesRef() {
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
    [[nodiscard]] outcome::result<BlockHash> computeLmdGhostHead(
        const BlockHash &start_root,
        const AttestationDataByValidator &attestations,
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
    outcome::result<void> validateAttestation(const Attestation &attestation);

    /**
     * Process a signed attestation received via gossip network.
     * This method:
     * 1. Verifies the XMSS signature
     * 2. Stores the signature in the gossip signature map
     * 3. Processes the attestation data via on_attestation
     * Args:
     *     signed_attestation: The signed attestation from gossip.
     */
    outcome::result<void> onGossipAttestation(
        const SignedAttestation &signed_attestation);

    /**
     * Process a signed aggregated attestation received via aggregation topic
     * This method:
     * 1. Verifies the aggregated attestation
     * 2. Stores the aggregation in aggregation_payloads map
     */
    outcome::result<void> onGossipAggregatedAttestation(
        const SignedAggregatedAttestation &signed_aggregated_attestation);
    outcome::result<void> onAggregatedAttestation(
        const SignedAggregatedAttestation &signed_aggregated_attestation,
        bool is_from_block);

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
    outcome::result<void> onAttestation(const Attestation &attestation,
                                        bool is_from_block);


    // Processes a new block, updates the store, and triggers a head update.
    outcome::result<void> onBlock(
        SignedBlockWithAttestation signed_block_with_attestation);

    using OnTickAction = std::variant<SignedAttestation,
                                      SignedAggregatedAttestation,
                                      SignedBlockWithAttestation>;

    // Advance forkchoice store time to given timestamp.
    // Ticks store forward interval by interval, performing appropriate
    // actions for each interval type.
    // Args:
    //    time: Target time in seconds since genesis.
    std::vector<OnTickAction> onTick(uint64_t now_sec);

    Interval time() const {
      return time_;
    }

   private:
    using ValidatorAttestationKey = std::tuple<ValidatorIndex, BlockHash>;

    struct SignaturesToAggregate {
      AttestationData data;
      // signatures and public keys must follow bitset order
      std::map<ValidatorIndex, Signature> signatures;
      bool aggregated = false;
    };

    void addSignatureToAggregate(const AttestationData &data,
                                 ValidatorIndex validator_index,
                                 const Signature &signature);

    /**
     * Compute map key for validator index and attestation data.
     */
    static ValidatorAttestationKey validatorAttestationKey(
        ValidatorIndex validator_index,
        const AttestationData &attestation_data);

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
    bool validateAggregatedSignature(
        const State &state,
        const AttestationData &attestation,
        const AggregatedSignatureProof &signature) const;

    // Compute aggregated signatures for a set of attestations.
    // This method implements a two-phase signature collection strategy:
    // 1. **Gossip Phase**: For each attestation group, first attempt to collect
    //     individual XMSS signatures from the gossip network. These are fresh
    //     signatures that validators broadcast when they attest.
    // 2. **Fallback Phase**: For any validators not covered by gossip, fall
    // back
    //     to previously-seen aggregated proofs from blocks. This uses a greedy
    //     set-cover approach to minimize the number of proofs needed.
    // The result is a list of (attestation, proof) pairs ready for block
    // inclusion.
    std::pair<AggregatedAttestations, AttestationSignatures>
    computeAggregatedSignatures(
        const State &state,
        const AggregatedAttestations &completely_aggregated_attestations);

    std::vector<SignedAggregatedAttestation> aggregateSignatures();

    void prune(Slot finalized_slot);

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
     * Checkpoint of the current canonical chain head block.
     *
     * This is the result of running the fork choice algorithm on the current
     * contents of the Store.
     */
    Checkpoint head_;

    /**
     * Root of the current safe target for attestation.
     *
     * This can be used by higher-level logic to restrict which blocks are
     * considered safe to attest to, based on additional safety conditions.
     */
    Checkpoint safe_target_;

    /**
     * For each known block, we keep its post-state.
     *
     * These states carry justified and finalized checkpoints that we use to
     * update the Store's latest justified and latest finalized checkpoints.
     */
    static constexpr int kStateCacheSize = 16;
    mutable LruCache<BlockHash, State> states_{kStateCacheSize};

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
    AttestationDataByValidator latest_known_attestations_;

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
    AttestationDataByValidator latest_new_attestations_;
    /**
     * Accumulates signatures to be aggregated in slot interval 2.
     * Grouped by attestation data.
     */
    std::unordered_map<Hash, SignaturesToAggregate> signatures_to_aggregate_;
    /**
     * Aggregated signature payloads for attestations from blocks.
     * - Keyed by (validator_id, attestation_data_root).
     * - Values are lists because same (validator_id, data) can appear in
     * multiple aggregations.
     * - Used for recursive signature aggregation when building blocks.
     * - Populated by on_block.
     */
    std::unordered_map<
        ValidatorAttestationKey,
        std::vector<std::shared_ptr<SignedAggregatedAttestation>>>
        aggregated_payloads_;
    qtils::SharedRef<ValidatorRegistry> validator_registry_;
    qtils::SharedRef<app::ValidatorKeysManifest> validator_keys_manifest_;
    /**
     * Index of the validator running this store instance.
     */
    ValidatorIndex validator_id_;
    bool is_aggregator_;
  };

}  // namespace lean
