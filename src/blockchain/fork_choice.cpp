/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/fork_choice.hpp"

#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>

#include <qtils/to_shared_ptr.hpp>
#include <qtils/value_or_raise.hpp>

#include "blockchain/genesis_config.hpp"
#include "blockchain/is_proposer.hpp"
#include "impl/block_tree_impl.hpp"
#include "metrics/impl/metrics_impl.hpp"
#include "types/signed_block_with_attestation.hpp"
#include "utils/lru_cache.hpp"

namespace lean {
  inline crypto::xmss::XmssMessage attestationPayload(
      const AttestationData &attestation_data) {
    return sszHash(attestation_data);
  }

  outcome::result<void> ForkChoiceStore::updateSafeTarget() {
    SL_TRACE(logger_, "Update safe target");
    // Get validator count from head state
    OUTCOME_TRY(head_state, getState(head_.root));

    // 2/3rd majority min voting weight for target selection
    auto min_target_score = ceilDiv(head_state->validatorCount() * 2, 3);

    OUTCOME_TRY(lmd_ghost_head,
                computeLmdGhostHead(block_tree_->getLatestJustified().root,
                                    latest_new_attestations_,
                                    min_target_score));

    OUTCOME_TRY(slot, getBlockSlot(lmd_ghost_head));

    safe_target_ = {.root = lmd_ghost_head, .slot = slot};
    SL_TRACE(logger_, "Safe target was set to {}", safe_target_);

    return outcome::success();
  }

  outcome::result<void> ForkChoiceStore::updateHead() {
    SL_TRACE(logger_, "Update head");
    // Run LMD-GHOST fork choice algorithm
    //
    // Selects canonical head by walking the tree from the justified root,
    // choosing the heaviest child at each fork based on attestation weights.
    OUTCOME_TRY(lmd_ghost_head,
                computeLmdGhostHead(block_tree_->getLatestJustified().root,
                                    latest_new_attestations_,
                                    0));

    OUTCOME_TRY(slot, getBlockSlot(lmd_ghost_head));

    head_ = {.root = lmd_ghost_head, .slot = slot};
    SL_TRACE(logger_, "Head was set to {}", head_);
    return outcome::success();
  }

  outcome::result<void> ForkChoiceStore::acceptNewAttestations() {
    SL_TRACE(logger_,
             "Accepting new {} attestations",
             latest_new_attestations_.size());
    for (auto &[validator, attestation] : latest_new_attestations_) {
      latest_known_attestations_[validator] = attestation;
    }
    latest_new_attestations_.clear();
    return updateHead();
  }

  Slot ForkChoiceStore::getCurrentSlot() const {
    Slot current_slot = time_ / INTERVALS_PER_SLOT;
    return current_slot;
  }

  Checkpoint ForkChoiceStore::getHead() {
    return head_;
  }

  outcome::result<std::shared_ptr<const State>> ForkChoiceStore::getState(
      const BlockHash &block_hash) const {
    SL_TRACE(logger_, "Getting state for block {}", block_hash);
    auto state = states_.get_else(block_hash, [&]() -> outcome::result<State> {
      SL_TRACE(logger_, "Loading state for block {}", block_hash);
      OUTCOME_TRY(state_opt, block_storage_->getState(block_hash));
      if (state_opt.has_value()) {
        SL_TRACE(logger_, "State for block {} was loaded", block_hash);
        return state_opt.value();
      }
      SL_TRACE(logger_, "State for block {} not found", block_hash);
      return Error::STATE_NOT_FOUND;
    });
    return state;
  }

  bool ForkChoiceStore::hasBlock(const BlockHash &hash) const {
    return block_tree_->has(hash);
  }

  outcome::result<Slot> ForkChoiceStore::getBlockSlot(
      const BlockHash &block_hash) const {
    return block_tree_->getSlotByHash(block_hash);
  }

  const Config &ForkChoiceStore::getConfig() const {
    return config_;
  }

  Checkpoint ForkChoiceStore::getLatestFinalized() const {
    return static_cast<Checkpoint>(block_tree_->lastFinalized());
  }

  Checkpoint ForkChoiceStore::getLatestJustified() const {
    return static_cast<Checkpoint>(block_tree_->getLatestJustified());
  }

  Checkpoint ForkChoiceStore::getAttestationTarget() const {
    // Start from head as target-candidate
    auto target_block_root = head_.root;

    // If there is no very recent safe target, then vote for the k'th ancestor
    // of the head
    const auto safe_target_slot = safe_target_.slot;

    // Ensure the attestation target is not older than the latest justified
    // block, as it would violate protocol rules and fail validation.
    auto latest_justified_slot = block_tree_->getLatestJustified().slot;
    auto lookback_limit = std::max(safe_target_slot, latest_justified_slot);

    for (auto i = 0; i < JUSTIFICATION_LOOKBACK_SLOTS; ++i) {
      auto target_header_res = block_tree_->getBlockHeader(target_block_root);
      if (target_header_res.has_error()) {
        SL_CRITICAL(logger_,
                    "Failed getting header of head or some it's ancestor: {}",
                    target_header_res.error());
        std::abort();  // Terminate to avoid breaking run
      }
      auto &target_header = target_header_res.value();
      if (target_header.slot > lookback_limit) {
        target_block_root = target_header.parent_root;
      } else {
        break;
      }
    }

    // If the latest finalized slot is very far back, then only some slots are
    // valid to justify, make sure the target is one of those
    auto latest_finalized_slot = block_tree_->lastFinalized().slot;
    while (true) {
      auto target_header_res = block_tree_->getBlockHeader(target_block_root);
      if (target_header_res.has_error()) {
        SL_CRITICAL(logger_,
                    "Failed getting header of some finalized block: {}",
                    target_header_res.error());
        std::abort();  // Terminate to avoid breaking run
      }
      auto &target_header = target_header_res.value();
      if (isJustifiableSlot(latest_finalized_slot, target_header.slot)) {
        break;
      }
      target_block_root = target_header.parent_root;
    }

    auto target_header_res = block_tree_->getBlockHeader(target_block_root);
    if (target_header_res.has_error()) {
      SL_CRITICAL(logger_,
                  "Failed getting header of target block: {}",
                  target_header_res.error());
      std::abort();  // Terminate to avoid breaking run
    }
    auto &target_header = target_header_res.value();
    return Checkpoint{
        .root = target_block_root,
        .slot = target_header.slot,
    };
  }

  AttestationData ForkChoiceStore::produceAttestationData(Slot slot) const {
    auto target_checkpoint = getAttestationTarget();
    auto source_checkpoint = getLatestJustified();

    return AttestationData{
        .slot = slot,
        .head = head_,
        .target = target_checkpoint,
        .source = source_checkpoint,
    };
  }

  outcome::result<SignedBlockWithAttestation>
  ForkChoiceStore::produceBlockWithSignatures(Slot slot,
                                              ValidatorIndex proposer_index) {
    // Get parent block and state to build upon
    auto head_root = head_.root;
    OUTCOME_TRY(head_state, getState(head_root));

    // Validate proposer authorization for this slot
    if (not isProposer(proposer_index, slot, head_state->validatorCount())) {
      return Error::INVALID_PROPOSER;
    }

    // Initialize empty attestation set for iterative collection
    AggregatedAttestations aggregated_attestations;
    std::unordered_map<BlockHash, size_t> aggregated_attestation_indices;

    // Iteratively collect valid attestations using fixed-point algorithm
    // Continue until no new attestations can be added to the block
    while (true) {
      // Create candidate block with current attestation set
      Block candidate_block{
          .slot = slot,
          .proposer_index = proposer_index,
          .parent_root = head_root,
          // Temporary; updated after state computation
          .state_root = {},
          .body = {.attestations = aggregated_attestations},
      };

      // Apply state transition to get the post-block state
      // First advance state to target-slot, then process the block
      auto post_state = *head_state;
      BOOST_OUTCOME_TRY(stf_.processSlots(post_state, slot));
      BOOST_OUTCOME_TRY(stf_.processBlock(post_state, candidate_block));

      // Find new valid attestations matching post-state justification
      auto new_attestations = false;
      for (auto &[validator_id, data] : latest_known_attestations_) {
        // Skip if the target block is unknown in our store
        if (not block_tree_->has(data.head.root)) {
          continue;
        }

        // Skip if attestation's source does not match post-state's latest
        // justified
        if (data.source != post_state.latest_justified) {
          continue;
        }

        auto key = validatorAttestationKey(validator_id, data);
        if (not gossip_attestation_signatures_.contains(key)
            and not aggregated_payloads_.contains(key)) {
          continue;
        }

        auto attestation_hash = attestationPayload(data);
        auto attestation_index_it =
            aggregated_attestation_indices.find(attestation_hash);
        if (attestation_index_it == aggregated_attestation_indices.end()) {
          attestation_index_it =
              aggregated_attestation_indices
                  .emplace(attestation_hash, aggregated_attestations.size())
                  .first;
          aggregated_attestations.data().emplace_back(
              AggregatedAttestation{.data = data});
        }
        auto attestation_index = attestation_index_it->second;
        auto &aggregated_attestation =
            aggregated_attestations.data().at(attestation_index);

        if (not hasAggregatedValidator(aggregated_attestation.aggregation_bits,
                                       validator_id)) {
          new_attestations = true;
          addAggregatedValidator(aggregated_attestation.aggregation_bits,
                                 validator_id);
        }
      }

      // Fixed point reached: no new attestations found
      if (not new_attestations) {
        break;
      }
    }

    // Compute the aggregated signatures for the attestations.
    // If the attestations cannot be aggregated, split it in a greedy way.
    auto aggregated =
        computeAggregatedSignatures(*head_state, aggregated_attestations);
    aggregated_attestations = aggregated.first;

    // Create the final block with all collected attestations
    Block block{
        .slot = slot,
        .proposer_index = proposer_index,
        .parent_root = head_root,
        // Will be updated with computed hash
        .state_root = {},
        .body = {.attestations = aggregated_attestations},
    };
    // Apply state transition to get final post-state and compute state root
    BOOST_OUTCOME_TRY(auto state,
                      stf_.stateTransition(block, *head_state, false));
    block.state_root = sszHash(state);
    block.setHash();

    auto proposer_attestation = produceAttestation(slot, proposer_index);
    proposer_attestation.data.head = Checkpoint::from(block);

    // Sign proposer attestation
    auto payload = attestationPayload(proposer_attestation.data);
    auto timer =
        metrics_->crypto_pq_signature_attestation_signing_time_seconds()
            ->timer();
    crypto::xmss::XmssSignature proposer_signature = xmss_provider_->sign(
        validator_keys_manifest_->currentNodeXmssKeypair().private_key,
        slot,
        payload);
    timer.stop();
    SignedBlockWithAttestation signed_block_with_attestation{
        .message =
            {
                .block = block,
                .proposer_attestation = proposer_attestation,
            },
        .signature =
            {
                .attestation_signatures = aggregated.second,
                .proposer_signature = proposer_signature,
            },
    };
    BOOST_OUTCOME_TRY(onBlock(signed_block_with_attestation));

    return signed_block_with_attestation;
  }

  Attestation ForkChoiceStore::produceAttestation(
      Slot slot, ValidatorIndex validator_index) const {
    return Attestation{
        .validator_id = validator_index,
        .data = produceAttestationData(slot),
    };
  }

  outcome::result<void> ForkChoiceStore::validateAttestation(
      const Attestation &attestation) {
    auto &data = attestation.data;

    SL_TRACE(logger_,
             "Validating attestation for target={}, source={}",
             data.target,
             data.source);
    auto timer = metrics_->fc_attestation_validation_time_seconds()->timer();

    // Availability Check

    // We cannot count a vote if we haven't seen the blocks involved.
    if (not block_tree_->has(data.source.root)) {
      return Error::CANT_VALIDATE_ATTESTATION_SOURCE_NOT_FOUND;
    }
    if (not block_tree_->has(data.target.root)) {
      return Error::CANT_VALIDATE_ATTESTATION_TARGET_NOT_FOUND;
    }
    if (not block_tree_->has(data.head.root)) {
      return Error::CANT_VALIDATE_ATTESTATION_HEAD_NOT_FOUND;
    }

    // Topology Check

    // History is linear and monotonic. Source must be older than Target.
    if (data.source.slot > data.target.slot) {
      SL_TRACE(logger_,
               "Invalid attestation: source slot {} > target slot {}",
               data.source,
               data.target);
      return Error::INVALID_ATTESTATION;
    }

    // Consistency Check

    // Validate checkpoint slots match block slots
    if (auto res = getBlockSlot(data.source.root);
        not res.has_value() or res.value() != data.source.slot) {
      SL_TRACE(logger_,
               "Invalid attestation: inconsistent source slot",
               data.target,
               data.source);
      return Error::INVALID_ATTESTATION;
    }
    if (auto res = getBlockSlot(data.target.root);
        not res.has_value() or res.value() != data.target.slot) {
      SL_TRACE(logger_,
               "Invalid attestation: inconsistent target slot",
               data.target,
               data.source);
      return Error::INVALID_ATTESTATION;
    }

    // Time Check

    // Validate attestation is not too far in the future
    // We allow a small margin for clock disparity (1 slot), but no further.
    if (data.slot > getCurrentSlot() + 1) {
      SL_TRACE(logger_,
               "Invalid attestation: too big clock disparity",
               data.target,
               data.source);
      return Error::INVALID_ATTESTATION;
    }

    return outcome::success();
  }

  outcome::result<void> ForkChoiceStore::onGossipAttestation(
      const SignedAttestation &signed_attestation) {
    Attestation attestation{
        .validator_id = signed_attestation.validator_id,
        .data = signed_attestation.message,
    };
    BOOST_OUTCOME_TRY(validateAttestation(attestation));
    OUTCOME_TRY(state, getState(signed_attestation.message.target.root));
    if (signed_attestation.validator_id >= state->validators.size()) {
      return Error::INVALID_ATTESTATION;
    }
    auto payload = attestationPayload(signed_attestation.message);
    if (not xmss_provider_->verify(
            state->validators[signed_attestation.validator_id].pubkey,
            payload,
            signed_attestation.message.slot,
            signed_attestation.signature)) {
      return Error::INVALID_ATTESTATION;
    }
    gossip_attestation_signatures_.emplace(
        validatorAttestationKey(signed_attestation.validator_id,
                                signed_attestation.message),
        signed_attestation.signature);
    return onAttestation(attestation, false);
  }

  outcome::result<void> ForkChoiceStore::onAttestation(
      const Attestation &attestation, bool is_from_block) {
    // First, ensure the attestation is structurally and temporally valid.
    auto source = is_from_block ? "block" : "gossip";

    // Extract node id
    auto node_id_opt =
        validator_registry_->nodeIdByIndex(attestation.validator_id);
    if (not node_id_opt.has_value()) {
      SL_WARN(logger_,
              "Received attestation from unknown validator index {}",
              attestation.validator_id);
    }

    if (auto res = validateAttestation(attestation); res.has_value()) {
      metrics_->fc_attestations_valid_total({{"source", source}})->inc();
      SL_DEBUG(logger_,
               "⚙️ Processing valid attestation from validator {} for "
               "target={}, source={}",
               node_id_opt ? node_id_opt.value() : "unknown",
               attestation.data.target,
               attestation.data.source);
    } else {
      metrics_->fc_attestations_invalid_total({{"source", source}})->inc();
      SL_WARN(logger_,
              "❌ Invalid attestation from validator {} for target={}, "
              "source={}: {}",
              node_id_opt ? node_id_opt.value() : "unknown",
              attestation.data.target,
              attestation.data.source,
              res.error());
      return res;
    }

    // Extract the validator index that produced this attestation.
    auto &validator_id = attestation.validator_id;

    // Extract the attestation's slot:
    // - used to decide if this attestation is "newer" than a previous one.
    auto &attestation_slot = attestation.data.slot;

    if (is_from_block) {
      // On-chain attestation processing

      // These are historical attestations from other validators included by the
      // proposer.
      // - They are processed immediately as "known" attestations,
      // - They contribute to fork choice weights.

      // Fetch the currently known attestation for this validator, if any.
      auto latest_known_attestation =
          latest_known_attestations_.find(validator_id);

      // Update the known attestation for this validator if:
      // - there is no known attestation yet, or
      // - this attestation is from a later slot than the known one.
      if (latest_known_attestation == latest_known_attestations_.end()
          or latest_known_attestation->second.slot < attestation_slot) {
        latest_known_attestations_.insert_or_assign(validator_id,
                                                    attestation.data);
      }

      // Fetch any pending ("new") attestation for this validator.
      auto latest_new_attestation = latest_new_attestations_.find(validator_id);

      // Remove the pending attestation if:
      // - it exists, and
      // - it is from an equal or earlier slot than this on-chain attestation.
      //
      // In that case, the on-chain attestation supersedes it.
      if (latest_new_attestation != latest_new_attestations_.end()
          and latest_new_attestation->second.slot <= attestation_slot) {
        latest_new_attestations_.erase(latest_new_attestation);
      }
    } else {
      // Network gossip attestation processing

      // These are attestations received via the gossip network.
      // - They enter the "new" stage,
      // - They must wait for interval tick acceptance before
      //   contributing to fork choice weights.

      // Convert Store time to slots to check for "future" attestations.
      auto time_slot = getCurrentSlot();

      // Reject the attestation if:
      // - its slot is strictly greater than our current slot.
      if (attestation_slot > time_slot) {
        return Error::INVALID_ATTESTATION;
      }

      // Fetch the previously stored "new" attestation for this validator.
      auto latest_new_attestation = latest_new_attestations_.find(validator_id);

      // Update the pending attestation for this validator if:
      // - there is no pending attestation yet, or
      // - this one is from a later slot than the pending one.
      if (latest_new_attestation == latest_new_attestations_.end()
          or latest_new_attestation->second.slot < attestation_slot) {
        latest_new_attestations_.insert_or_assign(validator_id,
                                                  attestation.data);
      }
    }

    return outcome::success();
  }

  bool ForkChoiceStore::validateBlockSignatures(
      const SignedBlockWithAttestation &signed_block) const {
    // Unpack the signed block components
    const auto &message = signed_block.message;
    const auto &block = message.block;
    const auto &signatures = signed_block.signature;

    // Combine all attestations that need verification

    // This creates a single list containing both:
    // 1. Block body attestations (from other validators)
    // 2. Proposer attestation (from the block producer)
    auto &aggregated_attestations = block.body.attestations;
    auto &attestation_signatures =
        signed_block.signature.attestation_signatures;

    // Verify signature count matches attestation count
    //
    // Each attestation must have exactly one corresponding signature.
    //
    // The ordering must be preserved:
    // 1. Block body attestations,
    // 2. The proposer attestation.
    if (attestation_signatures.size() != aggregated_attestations.size()) {
      SL_WARN(logger_,
              "Number of signatures does not match number of attestations: "
              "{} signatures != {} attestations",
              attestation_signatures.size(),
              aggregated_attestations.size());
      return false;
    }

    // Retrieve parent state to access validator public keys
    //
    // We use the parent state because:
    // - Validator set is determined at the parent block
    // - Public keys must be registered before signing
    // - State root is committed in the block header
    auto parent_state_res = getState(block.parent_root);
    if (parent_state_res.has_error()) {
      SL_WARN(logger_,
              "Parent state not found for block {}: {}",
              block.index(),
              parent_state_res.error());
      return false;
    }
    const auto &validators = parent_state_res.value()->validators;


    for (auto &&[aggregated_attestation, aggregated_signature] :
         std::views::zip(aggregated_attestations, attestation_signatures)) {
      std::vector<crypto::xmss::XmssPublicKey> public_keys;
      for (auto &&validator_id :
           getAggregatedValidators(aggregated_attestation.aggregation_bits)) {
        if (validator_id >= validators.size()) {
          SL_WARN(logger_, "Validator index out of range");
          return false;
        }
        public_keys.emplace_back(validators.data().at(validator_id).pubkey);
      }

      // Verify the XMSS signature
      //
      // This cryptographically proves that:
      // - The validator possesses the secret key for their public key
      // - The attestation has not been tampered with
      // - The signature was created at the correct epoch (slot)
      auto message = attestationPayload(aggregated_attestation.data);
      Epoch epoch = aggregated_attestation.data.slot;

      auto timer =
          metrics_
              ->crypto_pq_signature_aggregated_attestation_verification_time_seconds()
              ->timer();
      bool verify_result = xmss_provider_->verifyAggregatedSignatures(
          public_keys,
          epoch,
          message,
          qtils::ByteVec{aggregated_signature.data()});
      timer.stop();

      if (not verify_result) {
        SL_WARN(logger_,
                "Attestation signature verification failed for validators {}",
                fmt::join(getAggregatedValidators(
                              aggregated_attestation.aggregation_bits),
                          " "));
        return false;
      }
    }
    auto payload = attestationPayload(message.proposer_attestation.data);

    auto timer =
        metrics_->crypto_pq_signature_attestation_verification_time_seconds()
            ->timer();
    bool verify_result = xmss_provider_->verify(
        validators.data().at(message.proposer_attestation.validator_id).pubkey,
        payload,
        message.proposer_attestation.data.slot,
        signed_block.signature.proposer_signature);
    timer.stop();

    if (not verify_result) {
      SL_WARN(logger_,
              "Attestation signature verification failed for validator {}",
              message.proposer_attestation.validator_id);
      return false;
    }
    SL_TRACE(
        logger_, "All block signatures are valid in block {}", block.index());
    return true;
  }

  outcome::result<void> ForkChoiceStore::onBlock(
      SignedBlockWithAttestation signed_block_with_attestation) {
    auto &block = signed_block_with_attestation.message.block;
    block.setHash();
    auto block_hash = block.hash();

    // If the block is already known, ignore it
    if (block_tree_->has(block_hash)) {
      return outcome::success();
    }

    auto &proposer_attestation =
        signed_block_with_attestation.message.proposer_attestation;
    auto &signatures = signed_block_with_attestation.signature;

    auto timer = metrics_->fc_block_processing_time_seconds()->timer();

    // Verify parent-chain is available

    // The parent state must exist before processing this block.
    // If missing, the node must sync the parent chain first.

    OUTCOME_TRY(parent_state, getState(block.parent_root));

    // at this point parent state should be available so node should sync
    // parent-chain if not available before adding block to forkchoice

    auto valid_signatures =
        validateBlockSignatures(signed_block_with_attestation);
    if (not valid_signatures) {
      SL_WARN(logger_, "Invalid signatures for block {}", block.index());
      return Error::INVALID_ATTESTATION;
    }

    // Get post-state from STF (State Transition Function)
    BOOST_OUTCOME_TRY(auto post_state,
                      stf_.stateTransition(block, *parent_state, true));

    // Add block
    SL_TRACE(logger_, "Adding block {} into block tree", block.index());
    OUTCOME_TRY(block_tree_->addBlock(signed_block_with_attestation));

    // Store state
    SL_TRACE(logger_, "Adding post-state for block {}", block.index());
    // OUTCOME_TRY(block_storage_->putState(block_hash, post_state));
    auto res = block_storage_->putState(block_hash, post_state);
    if (res.has_error()) {
      SL_WARN(
          logger_, "Failed to store post-state for block {}", block.index());
    } else {
      SL_TRACE(logger_, "Stored post-state for block {}", block.index());
    }

    // If post-state has a higher justified checkpoint, update it to the store.
    if (post_state.latest_justified.slot
        > block_tree_->getLatestJustified().slot) {
      OUTCOME_TRY(block_tree_->setJustified(post_state.latest_justified.root));
      SL_INFO(logger_, "🔒 Justified block: {}", post_state.latest_finalized);
    }

    // If post-state has a higher finalized checkpoint, update it to the store.
    if (post_state.latest_finalized.slot > block_tree_->lastFinalized().slot) {
      OUTCOME_TRY(block_tree_->finalize(post_state.latest_finalized.root));
      SL_INFO(logger_, "🔒 Finalized block: {}", post_state.latest_finalized);
    }

    // Cache state
    states_.put(block_hash, post_state);

    // Process block body attestations
    auto &aggregated_attestations =
        signed_block_with_attestation.message.block.body.attestations;
    auto &attestation_signatures =
        signed_block_with_attestation.signature.attestation_signatures;
    if (attestation_signatures.size() != aggregated_attestations.size()) {
      return Error::SIGNATURE_COUNT_MISMATCH;
    }
    for (auto &&[aggregated_attestation, aggregated_signature] :
         std::views::zip(aggregated_attestations, attestation_signatures)) {
      auto shared_aggregated_payload = qtils::toSharedPtr(
          std::make_pair(aggregated_attestation, aggregated_signature));
      for (auto &&validator_id :
           getAggregatedValidators(aggregated_attestation.aggregation_bits)) {
        // Store the aggregated signature payload against (validator_id,
        // data_root) This is a list because the same (validator_id, data) can
        // appear in multiple aggregated attestations, especially when we have
        // aggregator roles. This list can be recursively aggregated by the
        // block proposer.
        aggregated_payloads_[validatorAttestationKey(
                                 validator_id, aggregated_attestation.data)]
            .emplace_back(shared_aggregated_payload);

        // Import the attestation data into forkchoice for latest votes
        BOOST_OUTCOME_TRY(onAttestation(
            Attestation{
                .validator_id = validator_id,
                .data = aggregated_attestation.data,
            },
            true));
      }
    }

    // Update fork-choice head based on new block and attestations

    // IMPORTANT: This must happen BEFORE processing proposer attestation
    // to prevent the proposer from gaining circular weight advantage.
    OUTCOME_TRY(updateHead());

    // Process proposer attestation as if received via gossip

    // The proposer casts their attestation in interval 1, after a block
    // proposal. This attestation should:
    // 1. NOT affect this block's fork choice position (processed as "new")
    // 2. Be available for inclusion in future blocks
    // 3. Influence fork choice only after interval 3 (end of slot)
    // We also store the proposer's signature for potential future block
    // building.
    gossip_attestation_signatures_.emplace(
        validatorAttestationKey(proposer_attestation.validator_id,
                                proposer_attestation.data),
        signed_block_with_attestation.signature.proposer_signature);
    OUTCOME_TRY(onAttestation(proposer_attestation, false));

    return outcome::success();
  }

  std::vector<std::variant<SignedAttestation, SignedBlockWithAttestation>>
  ForkChoiceStore::onTick(uint64_t now_sec) {
    auto time_since_genesis = now_sec - config_.genesis_time;

    auto head_state_res = getState(head_.root);
    if (head_state_res.has_error()) {
      SL_CRITICAL(logger_,
                  "Fatal error: Failed getting state of head ({}): {}",
                  head_state_res.error());
      std::abort();  // Terminate to avoid breaking run
    }
    auto &head_state = head_state_res.value();

    auto validator_count = head_state->validatorCount();

    std::vector<std::variant<SignedAttestation, SignedBlockWithAttestation>>
        result{};
    while (time_ <= time_since_genesis) {
      Slot current_slot = time_ / INTERVALS_PER_SLOT;
      [[unlikely]] if (current_slot == 0) {
        // Skip actions for slot zero, which is the genesis slot
        time_ += 1;
        continue;
      }
      if (time_ % INTERVALS_PER_SLOT == 0) {
        // Slot start
        SL_DEBUG(logger_,
                 "Slot {} started with time {}",
                 current_slot,
                 time_ * SECONDS_PER_INTERVAL);
        auto producer_index = current_slot % validator_count;
        auto is_producer =
            validator_registry_->currentValidatorIndices().contains(
                producer_index);

        if (is_producer) {
          SL_TRACE(logger_,
                   "Interval 0 of slot {}: node is producer - try to produce",
                   current_slot);
          auto ana_res = acceptNewAttestations();
          if (ana_res.has_error()) {
            SL_WARN(logger_,
                    "Failed to accept new attestations: {}",
                    ana_res.error());
          }

          SL_TRACE(logger_,
                   "Trying to produced block on slot {} by producer index {}",
                   current_slot,
                   producer_index);
          auto res = produceBlockWithSignatures(current_slot, producer_index);
          if (!res.has_value()) {
            SL_ERROR(logger_,
                     "Failed to produce block for slot {}: {}",
                     current_slot,
                     res.error());
            time_ += 1;
            continue;
          }
          auto &produced_block = res.value();

          SL_TRACE(logger_,
                   "👷 Produced block {} with parent {} and state {}",
                   produced_block.message.block.index(),
                   produced_block.message.block.parent_root,
                   produced_block.message.block.state_root);
          result.emplace_back(std::move(produced_block));

        } else {
          SL_TRACE(logger_,
                   "Interval 0 of slot {}: node isn't producer - skip",
                   current_slot);
        }

      } else if (time_ % INTERVALS_PER_SLOT == 1) {
        SL_TRACE(logger_, "Interval 1 of slot {}", current_slot);

        // Ensure the head is updated before voting
        auto ana_res = acceptNewAttestations();
        if (ana_res.has_error()) {
          SL_WARN(logger_,
                  "Failed to accept new attestations: {}",
                  ana_res.error());
        }

        metrics_->fc_head_slot()->set(head_.slot);
        Checkpoint head = head_;
        auto target = getAttestationTarget();
        auto source = block_tree_->getLatestJustified();
        SL_INFO(logger_, "🔷 Head={}", head);
        SL_INFO(logger_, "🎯 Target={}", target);
        SL_INFO(logger_, "📌 Source={}", source);

        if (source.slot > target.slot) {
          SL_WARN(logger_,
                  "Attestation source slot {} is not less than target slot {}",
                  source.slot,
                  target.slot);
          time_ += 1;
          continue;
        }

        for (auto validator_index :
             validator_registry_->currentValidatorIndices()) {
          if (isProposer(validator_index, current_slot, validator_count)) {
            continue;
          }
          auto attestation = produceAttestation(current_slot, validator_index);
          // sign attestation
          auto payload = attestationPayload(attestation.data);
          crypto::xmss::XmssKeypair keypair =
              validator_keys_manifest_->currentNodeXmssKeypair();
          auto timer =
              metrics_->crypto_pq_signature_attestation_signing_time_seconds()
                  ->timer();
          crypto::xmss::XmssSignature signature =
              xmss_provider_->sign(keypair.private_key, current_slot, payload);
          timer.stop();
          SignedAttestation signed_attestation{
              .validator_id = validator_index,
              .message = attestation.data,
              .signature = signature,
          };

          // Dispatching send signed vote-only broadcasts to other peers.
          // Current peer should process attestation directly
          auto res = onAttestation(attestation, false);
          if (not res.has_value()) {
            SL_ERROR(logger_,
                     "Failed to process attestation for slot {}: {}",
                     current_slot,
                     res.error());
            continue;
          }
          SL_DEBUG(logger_,
                   "Produced vote for target={}",
                   signed_attestation.message.target);
          result.emplace_back(signed_attestation);
        }

      } else if (time_ % INTERVALS_PER_SLOT == 2) {
        SL_TRACE(logger_,
                 "Interval 2 of slot {}: update safe-target ",
                 current_slot);

        auto res = updateSafeTarget();
        if (res.has_error()) {
          SL_WARN(logger_, "Failed to update safe-target: {}", res.error());
        }

      } else if (time_ % INTERVALS_PER_SLOT == 3) {
        SL_TRACE(logger_,
                 "Interval 3 of slot {}: accepting new attestations",
                 current_slot);

        auto ana_res = acceptNewAttestations();
        if (ana_res.has_error()) {
          SL_WARN(logger_,
                  "Failed to accept new attestations: {}",
                  ana_res.error());
        }
      }
      time_ += 1;
    }
    return result;
  }

  outcome::result<BlockHash> ForkChoiceStore::computeLmdGhostHead(
      const BlockHash &start_root,
      const AttestationDataByValidator &attestations,
      uint64_t min_score) const {
    // If the starting point is not defined, choose last finalized as an anchor;
    // don’t descend below finality

    // This ensures that the walk always has an anchor.
    auto anchor = start_root;
    if (anchor == kZeroHash or not block_tree_->has(anchor)) {
      anchor = block_tree_->lastFinalized().hash;
    }

    // Remember the slot of the anchor once and reuse it during the walk.

    // This avoids repeated lookups inside the inner loop.
    // const auto start_slot = blocks_.at(anchor).message.block.slot;
    OUTCOME_TRY(start_slot, getBlockSlot(anchor));

    // Prepare a table that will collect voting weight for each block.

    // Each entry starts conceptually at zero and then accumulates
    // contributions.
    std::unordered_map<BlockHash, uint64_t> weights;
    auto get_weight = [&](const BlockHash &hash) {
      auto it = weights.find(hash);
      return it != weights.end() ? it->second : 0;
    };

    // For every vote, follow the chosen head upward through its ancestors.

    // Each visited block accumulates one unit of weight from that validator.
    for (auto &attestation : attestations | std::views::values) {
      // Climb towards the anchor while staying inside the known tree.
      // This naturally handles partial views and ongoing sync.
      for (auto current = attestation.head.root;;) {
        auto current_header_res = block_tree_->tryGetBlockHeader(current);
        if (current_header_res.has_failure()) {
          break;
        }
        auto &current_header_opt = current_header_res.value();
        if (!current_header_opt) {
          break;
        }
        auto &current_header = current_header_opt.value();
        if (current_header.slot <= start_slot) {
          break;
        }
        ++weights[current];
        current = current_header.parent_root;
      }
    }

    auto head = anchor;
    for (;;) {
      auto children_res = block_tree_->getChildren(head);
      if (children_res.has_failure()) {
        return head;
      }
      auto &children = children_res.value();
      if (children.empty()) {
        return head;
      }

      // Heuristic check: prune branches early if they lack sufficient weight
      if (min_score > 0) {
        std::erase_if(children, [&](const BlockHash &hash) {
          return get_weight(hash) < min_score;
        });
        if (children.empty()) {
          return head;
        }
      }

      // Choose best child: most attestations, then lexicographically highest
      // hash
      head = *std::max_element(
          children.begin(),
          children.end(),
          [&get_weight](const BlockHash &lhs, const BlockHash &rhs) {
            auto lhs_weight = get_weight(lhs);
            auto rhs_weight = get_weight(rhs);
            if (lhs_weight == rhs_weight) {
              return lhs < rhs;
            }
            return lhs_weight < rhs_weight;
          });
    }
  }

  ForkChoiceStore::ForkChoiceStore(
      qtils::SharedRef<AnchorState> anchor_state,
      qtils::SharedRef<AnchorBlock> anchor_block,
      qtils::SharedRef<clock::SystemClock> clock,
      qtils::SharedRef<log::LoggingSystem> logging_system,
      qtils::SharedRef<metrics::Metrics> metrics,
      qtils::SharedRef<ValidatorRegistry> validator_registry,
      qtils::SharedRef<app::ValidatorKeysManifest> validator_keys_manifest,
      qtils::SharedRef<crypto::xmss::XmssProvider> xmss_provider,
      qtils::SharedRef<blockchain::BlockTree> block_tree,
      qtils::SharedRef<blockchain::BlockStorage> block_storage)
      : logger_(logging_system->getLogger("ForkChoice", "fork_choice")),
        metrics_(std::move(metrics)),
        xmss_provider_(std::move(xmss_provider)),
        block_tree_(std::move(block_tree)),
        block_storage_(std::move(block_storage)),
        stf_(metrics_, logging_system->getLogger("STF", "stf")),
        config_(anchor_state->config),
        validator_registry_(std::move(validator_registry)),
        validator_keys_manifest_(std::move(validator_keys_manifest)) {
    SL_TRACE(logger_, "Initialise fork-choice");

    for (auto xmss_pubkey : validator_keys_manifest_->getAllXmssPubkeys()) {
      SL_INFO(logger_, "Validator pubkey: {}", xmss_pubkey.toHex());
    }
    SL_INFO(
        logger_,
        "Our pubkey: {}",
        validator_keys_manifest_->currentNodeXmssKeypair().public_key.toHex());

    BOOST_ASSERT(anchor_block->state_root == sszHash(*anchor_state));
    anchor_block->setHash();
    SL_TRACE(logger_, "Anchor block: {}", anchor_block->index());
    SL_TRACE(logger_, "Anchor state: {}", anchor_block->state_root);

    auto now_sec = clock->nowSec();
    time_ = now_sec > config_.genesis_time
              ? (now_sec - config_.genesis_time) / SECONDS_PER_INTERVAL
              : 0;

    // Set last finalized as pre-initial-head
    auto latest_finalized = block_tree_->lastFinalized();
    SL_TRACE(logger_, "Last finalized: {}", head_);

    auto latest_justified = block_tree_->getLatestJustified();
    SL_TRACE(logger_, "Last justified: {}", latest_justified);

    head_ = latest_justified;

    // Init safe-target
    if (auto res = updateSafeTarget(); res.has_error()) {
      SL_WARN(logger_,
              "Failed initial safe-target update: {}; No changed ",
              res.error());
    }

    // Update head based on anchor block and state
    if (auto res = updateHead(); res.has_error()) {
      SL_WARN(
          logger_, "Failed initial head update: {}; No changed ", res.error());
    }

    SL_INFO(logger_, "🔷 Head:   {}", head_);
    SL_INFO(logger_, "🎯 Target: {}", safe_target_);
    SL_INFO(logger_, "📌 Source: {}", latest_justified);

    SL_TRACE(logger_, "Fork-choice initialized");
  }

  // Test constructor implementation
  ForkChoiceStore::ForkChoiceStore(
      uint64_t now_sec,
      qtils::SharedRef<log::LoggingSystem> logging_system,
      qtils::SharedRef<metrics::Metrics> metrics,
      Config config,
      Checkpoint head,
      Checkpoint safe_target,
      AttestationDataByValidator latest_known_attestations,
      AttestationDataByValidator latest_new_attestations,
      ValidatorIndex validator_index,
      qtils::SharedRef<ValidatorRegistry> validator_registry,
      qtils::SharedRef<app::ValidatorKeysManifest> validator_keys_manifest,
      qtils::SharedRef<crypto::xmss::XmssProvider> xmss_provider,
      qtils::SharedRef<blockchain::BlockTree> block_tree,
      qtils::SharedRef<blockchain::BlockStorage> block_storage)
      : logger_(logging_system->getLogger("ForkChoice", "fork_choice")),
        metrics_(metrics),
        xmss_provider_(std::move(xmss_provider)),
        block_tree_(std::move(block_tree)),
        block_storage_(std::move(block_storage)),
        stf_(std::move(metrics), logging_system->getLogger("STF", "stf")),
        time_(now_sec / SECONDS_PER_INTERVAL),
        config_(config),
        head_(head),
        safe_target_(safe_target),
        latest_known_attestations_(std::move(latest_known_attestations)),
        latest_new_attestations_(std::move(latest_new_attestations)),
        validator_registry_(std::move(validator_registry)),
        validator_keys_manifest_(std::move(validator_keys_manifest)) {}

  ForkChoiceStore::ValidatorAttestationKey
  ForkChoiceStore::validatorAttestationKey(
      ValidatorIndex validator_index, const AttestationData &attestation_data) {
    return {
        validator_index,
        sszHash(attestation_data),
    };
  }

  std::pair<AggregatedAttestations, AttestationSignatures>
  ForkChoiceStore::computeAggregatedSignatures(
      const State &state,
      const AggregatedAttestations &completely_aggregated_attestations) {
    AggregatedAttestations aggregated_attestations;
    AttestationSignatures aggregated_signatures;
    // Group individual attestations by data
    // Multiple validators may attest to the same data (slot, head, target,
    // source). We aggregate them into groups so each group can share a single
    // proof.
    for (auto &aggregated_attestation : completely_aggregated_attestations) {
      AggregationBits aggregation_bits;
      std::vector<crypto::xmss::XmssPublicKey> public_keys;
      std::vector<Signature> signatures;
      // Track validators we couldn't find signatures for.
      // These will need to be covered by Phase 2 (existing proofs).
      std::set<ValidatorIndex> remaining_validators;
      // Phase 1: Gossip Collection
      // When a validator creates an attestation, it broadcasts the
      // individual XMSS signature over the gossip network. If we have
      // received these signatures, we can aggregate them ourselves.
      // This is the preferred path: fresh signatures from the network.
      for (auto &&validator_id :
           getAggregatedValidators(aggregated_attestation.aggregation_bits)) {
        auto key =
            validatorAttestationKey(validator_id, aggregated_attestation.data);
        auto signatures_it = gossip_attestation_signatures_.find(key);
        // Attempt to collect each validator's signature from gossip.
        // Signatures are keyed by (validator ID, data root).
        // - If a signature exists, we add it to our collection.
        // - Otherwise, we mark that validator as "remaining" for the fallback
        // phase.
        if (signatures_it != gossip_attestation_signatures_.end()) {
          // Found a signature: collect it along with the public key.
          addAggregatedValidator(aggregation_bits, validator_id);
          public_keys.emplace_back(
              state.validators.data().at(validator_id).pubkey);
          signatures.emplace_back(signatures_it->second);
        } else {
          // No signature available: mark for fallback coverage.
          remaining_validators.emplace(validator_id);
        }
      }
      // If we collected any gossip signatures, aggregate them into a proof.
      // The aggregation combines multiple XMSS signatures into a single
      // compact proof that can verify all participants signed the message.
      if (not signatures.empty()) {
        auto payload = attestationPayload(aggregated_attestation.data);
        auto aggregated_signature = xmss_provider_->aggregateSignatures(
            public_keys, signatures, aggregated_attestation.data.slot, payload);
        aggregated_attestations.push_back(AggregatedAttestation{
            .aggregation_bits = aggregation_bits,
            .data = aggregated_attestation.data,
        });
        aggregated_signatures.push_back(aggregated_signature);
      }
      // Phase 2: Fallback to existing proofs
      // Some validators may not have broadcast their signatures over gossip,
      // but we might have seen proofs for them in previously-received blocks.
      // Example scenario:
      //   - We need signatures from validators {0, 1, 2, 3, 4}.
      //   - Gossip gave us signatures for {0, 1}.
      //   - Remaining: {2, 3, 4}.
      //   - From old blocks, we have:
      //       • Proof A covering {2, 3}
      //       • Proof B covering {3, 4}
      //       • Proof C covering {4}
      // We want to cover {2, 3, 4} with as few proofs as possible.
      // A greedy approach: always pick the proof with the largest overlap.
      //   - Iteration 1: Proof A covers {2, 3} (2 validators). Pick it.
      //                  Remaining: {4}.
      //   - Iteration 2: Proof B covers {4} (1 validator). Pick it.
      //                  Remaining: {} → done.
      // Result: 2 proofs instead of 3.
      while (not remaining_validators.empty()) {
        auto first_validator = *remaining_validators.begin();
        auto key = validatorAttestationKey(first_validator,
                                           aggregated_attestation.data);
        auto aggregated_payload_it = aggregated_payloads_.find(key);
        if (aggregated_payload_it == aggregated_payloads_.end()) {
          // No proofs found for this validator: search for next validator.
          remaining_validators.erase(first_validator);
          continue;
        }
        // Step 2: Pick the proof covering the most remaining validators.
        // At each step, we select the single proof that eliminates the highest
        // number of *currently missing* validators from our list.
        // The 'score' of a candidate proof is defined as the size of the
        // intersection between:
        //   A. The validators inside the proof (`p.participants`)
        //   B. The validators we still need (`remaining`)
        // Example:
        //   Remaining needed : {Alice, Bob, Charlie}
        //   Proof 1 covers   : {Alice, Dave}         -> Score: 1 (Only Alice
        //   counts) Proof 2 covers   : {Bob, Charlie, Eve}   -> Score: 2 (Bob &
        //   Charlie count)
        //   -> Result: We pick Proof 2 because it has the highest score.
        size_t best_overlap = 0;
        std::shared_ptr<AggregatedPayload> best_payload;
        for (auto &payload : aggregated_payload_it->second) {
          size_t overlap = 0;
          for (auto &&validator_index :
               getAggregatedValidators(payload->first.aggregation_bits)) {
            if (remaining_validators.contains(validator_index)) {
              ++overlap;
            }
          }
          if (overlap <= best_overlap) {
            continue;
          }
          best_overlap = overlap;
          best_payload = payload;
        }
        BOOST_ASSERT(best_payload != nullptr);
        // Step 3: Record the proof and remove covered validators.
        // In the future, we should be able to aggregate the proofs into a
        // single proof.
        for (auto &&validator_index :
             getAggregatedValidators(best_payload->first.aggregation_bits)) {
          remaining_validators.erase(validator_index);
        }
        aggregated_attestations.push_back(best_payload->first);
        aggregated_signatures.push_back(best_payload->second);
      }
    }
    return std::make_pair(aggregated_attestations, aggregated_signatures);
  }
}  // namespace lean
