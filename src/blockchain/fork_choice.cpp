/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/fork_choice.hpp"

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>

#include <boost/beast/http/verb.hpp>

#include "blockchain/genesis_config.hpp"
#include "blockchain/is_proposer.hpp"
#include "impl/block_tree_impl.hpp"
#include "metrics/impl/metrics_impl.hpp"
#include "types/signed_block_with_attestation.hpp"
#include "utils/lru_cache.hpp"

namespace lean {
  outcome::result<void> ForkChoiceStore::updateSafeTarget() {
    SL_TRACE(logger_, "Update safe target");
    // Get validator count from head state
    OUTCOME_TRY(head_state, getState(head_.root));

    // 2/3rd majority min voting weight for target selection
    auto min_target_score = ceilDiv(head_state->validatorCount() * 2, 3);

    auto lmd_ghost_head = computeLmdGhostHead(
        latest_justified_.root, latest_new_attestations_, min_target_score);

    auto slot_opt = getBlockSlot(lmd_ghost_head);
    BOOST_ASSERT(slot_opt.has_value());

    Checkpoint safe_target = {.root = lmd_ghost_head, .slot = slot_opt.value()};

    SL_TRACE(logger_, "Safe target was set to {}", safe_target);
    safe_target_ = safe_target.root;
    return outcome::success();
  }

  void ForkChoiceStore::updateHead() {
    SL_TRACE(logger_, "Update head");
    // Run LMD-GHOST fork choice algorithm
    //
    // Selects canonical head by walking the tree from the justified root,
    // choosing the heaviest child at each fork based on attestation weights.
    auto lmd_ghost_head = computeLmdGhostHead(
        latest_justified_.root, latest_known_attestations_, 0);

    auto slot_opt = getBlockSlot(lmd_ghost_head);
    BOOST_ASSERT(slot_opt.has_value());

    head_ = {.root = lmd_ghost_head, .slot = slot_opt.value()};
    SL_TRACE(logger_, "Head was set to {}", head_);
  }

  void ForkChoiceStore::acceptNewAttestations() {
    SL_TRACE(logger_,
             "Accepting new {} attestations",
             latest_new_attestations_.size());
    for (auto &[validator, attestation] : latest_new_attestations_) {
      latest_known_attestations_[validator] = attestation;
    }
    latest_new_attestations_.clear();
    updateHead();
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

  std::optional<Slot> ForkChoiceStore::getBlockSlot(
      const BlockHash &block_hash) const {
    auto slot_res = block_tree_->getSlotByHash(block_hash);
    if (slot_res.has_value()) {
      return slot_res.value();
    }
    return std::nullopt;
  }

  const Config &ForkChoiceStore::getConfig() const {
    return config_;
  }

  Checkpoint ForkChoiceStore::getLatestFinalized() const {
    return static_cast<Checkpoint>(block_tree_->lastFinalized());
  }

  Checkpoint ForkChoiceStore::getLatestJustified() const {
    return latest_justified_;
  }

  Checkpoint ForkChoiceStore::getAttestationTarget() const {
    // Start from head as target-candidate
    auto target_block_root = head_.root;

    // If there is no very recent safe target, then vote for the k'th ancestor
    // of the head
    auto safe_target_slot = getBlockSlot(safe_target_).value();
    for (auto i = 0; i < JUSTIFICATION_LOOKBACK_SLOTS; ++i) {
      auto target_header =
          block_tree_->getBlockHeader(target_block_root).value();
      if (target_header.slot > safe_target_slot) {
        target_block_root = target_header.parent_root;
      } else {
        break;
      }
    }

    // If the latest finalized slot is very far back, then only some slots are
    // valid to justify, make sure the target is one of those
    auto latest_finalized_slot = block_tree_->lastFinalized().slot;
    while (true) {
      auto target_header =
          block_tree_->getBlockHeader(target_block_root).value();

      if (isJustifiableSlot(latest_finalized_slot, target_header.slot)) {
        break;
      }
      target_block_root = target_header.parent_root;
    }

    auto target_header = block_tree_->getBlockHeader(target_block_root).value();
    return Checkpoint{
        .root = target_block_root,
        .slot = target_header.slot,
    };
  }

  AttestationData ForkChoiceStore::produceAttestationData(Slot slot) const {
    auto target_checkpoint = getAttestationTarget();

    return AttestationData{
        .slot = slot,
        .head = head_,
        .target = target_checkpoint,
        .source = latest_justified_,
    };
  }

  outcome::result<SignedBlockWithAttestation>
  ForkChoiceStore::produceBlockWithSignatures(Slot slot,
                                              ValidatorIndex validator_index) {
    // Get parent block and state to build upon
    auto head_root = head_.root;
    OUTCOME_TRY(head_state, getState(head_root));

    // Validate proposer authorization for this slot
    if (not isProposer(validator_index, slot, head_state->validatorCount())) {
      return Error::INVALID_PROPOSER;
    }

    // Initialize empty attestation set for iterative collection
    Attestations attestations;
    BlockSignatures signatures;

    // Iteratively collect valid attestations using fixed-point algorithm
    // Continue until no new attestations can be added to the block
    while (true) {
      // Create candidate block with current attestation set
      Block candidate_block{
          .slot = slot,
          .proposer_index = validator_index,
          .parent_root = head_root,
          // Temporary; updated after state computation
          .state_root = {},
          .body = {.attestations = attestations},
      };

      // Apply state transition to get the post-block state
      // First advance state to target-slot, then process the block
      auto post_state = *head_state;
      BOOST_OUTCOME_TRY(stf_.processSlots(post_state, slot));
      BOOST_OUTCOME_TRY(stf_.processBlock(post_state, candidate_block));

      // Find new valid attestations matching post-state justification
      auto new_attestations = false;
      for (auto &signed_attestation :
           latest_known_attestations_ | std::views::values) {
        // Skip if the target block is unknown in our store
        auto &data = signed_attestation.message.data;
        if (not block_tree_->has(data.head.root)) {
          continue;
        }

        // Skip if attestation's source does not match post-state's latest
        // justified
        if (data.source != post_state.latest_justified) {
          continue;
        }

        if (not std::ranges::contains(attestations,
                                      signed_attestation.message)) {
          new_attestations = true;
          attestations.push_back(signed_attestation.message);
          signatures.push_back(signed_attestation.signature);
        }
      }

      // Fixed point reached: no new attestations found
      if (not new_attestations) {
        break;
      }
    }

    // Create the final block with all collected attestations
    Block block{
        .slot = slot,
        .proposer_index = validator_index,
        .parent_root = head_root,
        // Will be updated with computed hash
        .state_root = {},
        .body = {.attestations = attestations},
    };
    // Apply state transition to get final post-state and compute state root
    BOOST_OUTCOME_TRY(auto state,
                      stf_.stateTransition(block, *head_state, false));
    block.state_root = sszHash(state);
    block.setHash();

    auto proposer_attestation = produceAttestation(slot, validator_index);
    proposer_attestation.data.head = Checkpoint::from(block);

    // Sign proposer attestation
    auto payload = sszHash(proposer_attestation);
    auto timer =
        metrics_->crypto_pq_signature_attestation_signing_time_seconds()
            ->timer();
    crypto::xmss::XmssSignature signature = xmss_provider_->sign(
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
        .signature = signatures,
    };
    signed_block_with_attestation.signature.data().push_back(signature);
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
      const SignedAttestation &signed_attestation) {
    auto &data = signed_attestation.message.data;

    SL_TRACE(logger_,
             "Validating attestation for target={}, source={}",
             data.target,
             data.source);
    auto timer = metrics_->fc_attestation_validation_time_seconds()->timer();

    // Availability Check

    // We cannot count a vote if we haven't seen the blocks involved.
    if (not block_tree_->has(data.source.root)) {
      return Error::INVALID_ATTESTATION;
    }
    if (not block_tree_->has(data.target.root)) {
      return Error::INVALID_ATTESTATION;
    }
    if (not block_tree_->has(data.head.root)) {
      return Error::INVALID_ATTESTATION;
    }

    // Topology Check

    // History is linear and monotonic. Source must be older than Target.
    if (data.source.slot > data.target.slot) {
      return Error::INVALID_ATTESTATION;
    }

    // Consistency Check

    // Validate checkpoint slots match block slots
    auto source_block_slot = getBlockSlot(data.source.root);
    auto target_block_slot = getBlockSlot(data.target.root);
    if (source_block_slot != data.source.slot) {
      return Error::INVALID_ATTESTATION;
    }
    if (target_block_slot != data.target.slot) {
      return Error::INVALID_ATTESTATION;
    }

    // Time Check

    // Validate attestation is not too far in the future
    // We allow a small margin for clock disparity (1 slot), but no further.
    if (data.slot > getCurrentSlot() + 1) {
      return Error::INVALID_ATTESTATION;
    }

    return outcome::success();
  }

  outcome::result<void> ForkChoiceStore::onAttestation(
      const SignedAttestation &signed_attestation, bool is_from_block) {
    // First, ensure the attestation is structurally and temporally valid.
    auto source = is_from_block ? "block" : "gossip";

    // Extract node id
    auto node_id_opt = validator_registry_->nodeIdByIndex(
        signed_attestation.message.validator_id);
    if (not node_id_opt.has_value()) {
      SL_WARN(logger_,
              "Received attestation from unknown validator index {}",
              signed_attestation.message.validator_id);
    }

    if (auto res = validateAttestation(signed_attestation); res.has_value()) {
      metrics_->fc_attestations_valid_total({{"source", source}})->inc();
      SL_DEBUG(logger_,
               "⚙️ Processing valid attestation from validator {} for "
               "target={}, source={}",
               node_id_opt ? node_id_opt.value() : "unknown",
               signed_attestation.message.data.target,
               signed_attestation.message.data.source);
    } else {
      metrics_->fc_attestations_invalid_total({{"source", source}})->inc();
      SL_WARN(logger_,
              "❌ Invalid attestation from validator {} for target={}, "
              "source={}: {}",
              node_id_opt ? node_id_opt.value() : "unknown",
              signed_attestation.message.data.target,
              signed_attestation.message.data.source,
              res.error());
      return res;
    }

    // Extract the validator index that produced this attestation.
    auto &validator_id = signed_attestation.message.validator_id;

    // Extract the attestation's slot:
    // - used to decide if this attestation is "newer" than a previous one.
    auto &attestation_slot = signed_attestation.message.data.slot;

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
          or latest_known_attestation->second.message.data.slot
                 < attestation_slot) {
        latest_known_attestations_.insert_or_assign(validator_id,
                                                    signed_attestation);
      }

      // Fetch any pending ("new") attestation for this validator.
      auto latest_new_attestation = latest_new_attestations_.find(validator_id);

      // Remove the pending attestation if:
      // - it exists, and
      // - it is from an equal or earlier slot than this on-chain attestation.
      //
      // In that case, the on-chain attestation supersedes it.
      if (latest_new_attestation != latest_new_attestations_.end()
          and latest_new_attestation->second.message.data.slot
                  <= attestation_slot) {
        latest_new_attestations_.erase(latest_new_attestation);
      }
    } else {
      // Network gossip attestation processing

      // These are attestations received via the gossip network.
      // - They enter the "new" stage,
      // - They must wait for interval tick acceptance before
      //   contributing to fork choice weights.

      // Convert Store time to slots to check for "future" attestations.
      auto time_slots = getCurrentSlot();

      // Reject the attestation if:
      // - its slot is strictly greater than our current slot.
      if (attestation_slot > time_slots) {
        return Error::INVALID_ATTESTATION;
      }

      // Fetch the previously stored "new" attestation for this validator.
      auto latest_new_attestation = latest_new_attestations_.find(validator_id);

      // Update the pending attestation for this validator if:
      // - there is no pending attestation yet, or
      // - this one is from a later slot than the pending one.
      if (latest_new_attestation == latest_new_attestations_.end()
          or latest_new_attestation->second.message.data.slot
                 < attestation_slot) {
        latest_new_attestations_.insert_or_assign(validator_id,
                                                  signed_attestation);
      }
    }

    return outcome::success();
  }

  inline bool isValidSignature(const Signature &signature) {
    return signature == Signature{};
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
    auto all_attestations = block.body.attestations;
    all_attestations.push_back(message.proposer_attestation);

    // Verify signature count matches attestation count
    //
    // Each attestation must have exactly one corresponding signature.
    //
    // The ordering must be preserved:
    // 1. Block body attestations,
    // 2. The proposer attestation.
    if (signatures.size() != all_attestations.size()) {
      SL_WARN(logger_,
              "Number of signatures does not match number of attestations");
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

    // Verify each attestation signature
    for (size_t index = 0; index < all_attestations.size(); ++index) {
      const auto &attestation = all_attestations[index];
      const auto &signature = signatures.data().at(index);

      // Identify the validator who created this attestation
      ValidatorIndex validator_id = attestation.validator_id;

      // Ensure validator exists in the active set
      if (validator_id >= validators.size()) {
        SL_WARN(logger_, "Validator index out of range");
        return false;
      }
      const auto &validator = validators[validator_id];

      // Verify the XMSS signature
      //
      // This cryptographically proves that:
      // - The validator possesses the secret key for their public key
      // - The attestation has not been tampered with
      // - The signature was created at the correct epoch (slot)
      auto message = sszHash(attestation);
      Epoch epoch = attestation.data.slot;

      auto timer =
          metrics_->crypto_pq_signature_attestation_verification_time_seconds()
              ->timer();
      bool verify_result =
          xmss_provider_->verify(validator.pubkey, message, epoch, signature);
      timer.stop();

      if (not verify_result) {
        SL_WARN(logger_,
                "Attestation signature verification failed for validator {}",
                validator_id);
        return false;
      }
    }
    SL_TRACE(
        logger_, "All block signatures are valid in block {}", block.index());
    return true;
  }

  void ForkChoiceStore::updateLastFinalized(const Checkpoint &checkpoint) {
    auto res = block_tree_->finalize(checkpoint.root);
    BOOST_ASSERT(not res.has_error());
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
      SL_WARN(logger_, "Failed to store post-state for block {}", block.index());
    } else {
      SL_TRACE(logger_, "Stored post-state for block {}", block.index());
    }

    // If post-state has a higher justified checkpoint, update it to the store.
    if (post_state.latest_justified.slot > latest_justified_.slot) {
      latest_justified_ = post_state.latest_justified;
    }

    // If post-state has a higher finalized checkpoint, update it to the store.
    if (post_state.latest_finalized.slot > block_tree_->lastFinalized().slot) {
      SL_INFO(logger_,
              "🔒 Finalized block={:0xx}, slot={}",
              post_state.latest_finalized.root,
              post_state.latest_finalized.slot);
      OUTCOME_TRY(block_tree_->finalize(post_state.latest_finalized.root));
    }

    // Cache state
    states_.put(block_hash, post_state);

    // Process block body attestations

    // Iterate over attestations and their corresponding signatures.
    for (size_t index = 0; index < block.body.attestations.size(); ++index) {
      if (index >= signatures.size()) {
        return Error::INVALID_ATTESTATION;
      }
      const auto &attestation = block.body.attestations[index];
      const auto &signature = signatures.data().at(index);

      // Process as on-chain attestation (immediately becomes "known")
      BOOST_OUTCOME_TRY(onAttestation(
          SignedAttestation{
              .message = attestation,
              .signature = signature,
          },
          true));
    }

    // Update forkchoice head based on new block and attestations

    // IMPORTANT: This must happen BEFORE processing proposer attestation
    // to prevent the proposer from gaining circular weight advantage.
    updateHead();

    // Process proposer attestation as if received via gossip

    // The proposer casts their attestation in interval 1, after block
    // proposal. This attestation should:
    // 1. NOT affect this block's fork choice position (processed as "new")
    // 2. Be available for inclusion in future blocks
    // 3. Influence fork choice only after interval 3 (end of slot)
    OUTCOME_TRY(onAttestation(
        SignedAttestation{
            .message = proposer_attestation,
            .signature = signatures.data().at(block.body.attestations.size()),
        },
        false));

    return outcome::success();
  }

  std::vector<std::variant<SignedAttestation, SignedBlockWithAttestation>>
  ForkChoiceStore::onTick(uint64_t now_sec) {
    auto time_since_genesis = now_sec - config_.genesis_time;

    auto head_state_res = getState(head_.root);
    BOOST_ASSERT(head_state_res.has_value());
    auto validator_count = head_state_res.value()->validatorCount();

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
                   "Interval 1 of slot {} - node is producer",
                   current_slot);
          acceptNewAttestations();

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
                   "Interval 1 of slot {} - node isn't producer - skip",
                   current_slot);
        }

      } else if (time_ % INTERVALS_PER_SLOT == 1) {
        SL_TRACE(logger_, "Interval 2 of slot{}", current_slot);

        metrics_->fc_head_slot()->set(head_.slot);
        Checkpoint head = head_;
        auto target = getAttestationTarget();

        SL_INFO(logger_, "🔷 Head={}", head);
        SL_INFO(logger_, "🎯 Target={}", target);
        SL_INFO(logger_, "📌 Source={}", latest_justified_);

        for (auto validator_index :
             validator_registry_->currentValidatorIndices()) {
          if (isProposer(validator_index, current_slot, validator_count)) {
            continue;
          }
          auto attestation = produceAttestation(current_slot, validator_index);
          // sign attestation
          auto payload = sszHash(attestation);
          crypto::xmss::XmssKeypair keypair =
              validator_keys_manifest_->currentNodeXmssKeypair();
          auto timer =
              metrics_->crypto_pq_signature_attestation_signing_time_seconds()
                  ->timer();
          crypto::xmss::XmssSignature signature =
              xmss_provider_->sign(keypair.private_key, current_slot, payload);
          timer.stop();
          SignedAttestation signed_attestation{.message = attestation,
                                               .signature = signature};

          // Dispatching send signed vote only broadcasts to other peers.
          // Current peer should process attestation directly
          auto res = onAttestation(signed_attestation, false);
          if (not res.has_value()) {
            SL_ERROR(logger_,
                     "Failed to process attestation for slot {}: {}",
                     current_slot,
                     res.error());
            continue;
          }
          SL_DEBUG(logger_,
                   "Produced vote for target={}",
                   signed_attestation.message.data.target);
          result.emplace_back(signed_attestation);
        }

      } else if (time_ % INTERVALS_PER_SLOT == 2) {
        SL_TRACE(logger_,
                 "Interval 3 of slot {} - update safe-target ",
                 current_slot);

        auto res = updateSafeTarget();
        if (res.has_error()) {
          SL_WARN(logger_, "Failed to update safe-target: {}", res.error());
        }

      } else if (time_ % INTERVALS_PER_SLOT == 3) {
        SL_TRACE(logger_,
                 "Interval 4 of slot {} - accepting new attestations",
                 current_slot);

        acceptNewAttestations();
      }
      time_ += 1;
    }
    return result;
  }

  BlockHash ForkChoiceStore::computeLmdGhostHead(
      const BlockHash &start_root,
      const SignedAttestations &attestations,
      uint64_t min_score) const {
    // If the starting point is not defined, choose the earliest known block.

    // This ensures that the walk always has an anchor.
    auto anchor = start_root;
    if (anchor == kZeroHash or not block_tree_->has(anchor)) {
      anchor = block_tree_->lastFinalized().hash;
    }

    // Remember the slot of the anchor once and reuse it during the walk.

    // This avoids repeated lookups inside the inner loop.
    // const auto start_slot = blocks_.at(anchor).message.block.slot;
    const auto start_slot = getBlockSlot(anchor);

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
      auto current = attestation.message.data.head.root;

      // Climb towards the anchor while staying inside the known tree.

      // This naturally handles partial views and ongoing sync.
      while (true) {
        auto current_header_res = block_tree_->tryGetBlockHeader(current);
        if (current_header_res.has_failure()) {
          break;
        }
        if (not current_header_res.value().has_value()) {
          break;
        }
        auto &current_header = current_header_res.value().value();
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
        validator_registry_(std::move(validator_registry)),
        validator_keys_manifest_(std::move(validator_keys_manifest)) {
    BOOST_ASSERT(anchor_block->state_root == sszHash(*anchor_state));
    anchor_block->setHash();
    config_ = anchor_state->config;
    auto now_sec = clock->nowSec();
    time_ = now_sec > config_.genesis_time
              ? (now_sec - config_.genesis_time) / SECONDS_PER_INTERVAL
              : 0;

    head_ = {.root = anchor_block->hash(), .slot = anchor_block->slot};
    safe_target_ = head_.root;

    // TODO: ensure latest justified and finalized are set correctly
    latest_justified_ = head_;

    for (auto xmss_pubkey : validator_keys_manifest_->getAllXmssPubkeys()) {
      SL_INFO(logger_, "Validator pubkey: {}", xmss_pubkey.toHex());
    }
    SL_INFO(
        logger_,
        "Our pubkey: {}",
        validator_keys_manifest_->currentNodeXmssKeypair().public_key.toHex());
  }

  // Test constructor implementation
  ForkChoiceStore::ForkChoiceStore(
      uint64_t now_sec,
      qtils::SharedRef<log::LoggingSystem> logging_system,
      qtils::SharedRef<metrics::Metrics> metrics,
      Config config,
      Checkpoint head,
      BlockHash safe_target,
      Checkpoint latest_justified,
      SignedAttestations latest_known_attestations,
      SignedAttestations latest_new_attestations,
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
        latest_justified_(latest_justified),
        latest_known_attestations_(std::move(latest_known_attestations)),
        latest_new_attestations_(std::move(latest_new_attestations)),
        validator_registry_(std::move(validator_registry)),
        validator_keys_manifest_(std::move(validator_keys_manifest)) {}
}  // namespace lean
