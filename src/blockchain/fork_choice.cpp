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
#include <vector>

#include "blockchain/genesis_config.hpp"
#include "blockchain/is_proposer.hpp"
#include "metrics/impl/metrics_impl.hpp"
#include "types/signed_block_with_attestation.hpp"

namespace lean {
  void ForkChoiceStore::updateSafeTarget() {
    // Get validator count from head state
    auto &head_state = getState(head_);

    // 2/3rd majority min voting voting weight for target selection
    auto min_target_score = ceilDiv(head_state.validatorCount() * 2, 3);

    safe_target_ = computeLmdGhostHead(
        latest_justified_.root, latest_new_attestations_, min_target_score);
  }

  void ForkChoiceStore::updateHead() {
    // Run LMD-GHOST fork choice algorithm
    //
    // Selects canonical head by walking the tree from the justified root,
    // choosing the heaviest child at each fork based on attestation weights.
    head_ = computeLmdGhostHead(
        latest_justified_.root, latest_known_attestations_, 0);
  }

  void ForkChoiceStore::acceptNewAttestations() {
    for (auto &[validator, attestation] : latest_new_attestations_) {
      latest_known_attestations_[validator] = attestation;
    }
    latest_new_attestations_.clear();
    updateHead();
  }

  Slot ForkChoiceStore::getCurrentSlot() {
    Slot current_slot = time_ / INTERVALS_PER_SLOT;
    return current_slot;
  }


  BlockHash ForkChoiceStore::getHead() {
    return head_;
  }

  const State &ForkChoiceStore::getState(const BlockHash &block_hash) const {
    auto it = states_.find(block_hash);
    if (it == states_.end()) {
      throw std::out_of_range("No state for block hash");
    }
    return it->second;
  }

  bool ForkChoiceStore::hasBlock(const BlockHash &hash) const {
    return blocks_.contains(hash);
  }

  std::optional<Slot> ForkChoiceStore::getBlockSlot(
      const BlockHash &block_hash) const {
    if (not blocks_.contains(block_hash)) {
      return std::nullopt;
    }
    return blocks_.at(block_hash).message.block.slot;
  }

  Slot ForkChoiceStore::getHeadSlot() const {
    return blocks_.at(head_).message.block.slot;
  }

  const Config &ForkChoiceStore::getConfig() const {
    return config_;
  }

  Checkpoint ForkChoiceStore::getLatestFinalized() const {
    return latest_finalized_;
  }

  Checkpoint ForkChoiceStore::getLatestJustified() const {
    return latest_justified_;
  }


  Checkpoint ForkChoiceStore::getAttestationTarget() const {
    // Start from head as target candidate
    auto target_block_root = head_;

    // If there is no very recent safe target, then vote for the k'th ancestor
    // of the head
    for (auto i = 0; i < JUSTIFICATION_LOOKBACK_SLOTS; ++i) {
      if (blocks_.at(target_block_root).message.block.slot
          > blocks_.at(safe_target_).message.block.slot) {
        target_block_root =
            blocks_.at(target_block_root).message.block.parent_root;
      } else {
        break;
      }
    }

    // If the latest finalized slot is very far back, then only some slots are
    // valid to justify, make sure the target is one of those
    while (not isJustifiableSlot(
        latest_finalized_.slot,
        blocks_.at(target_block_root).message.block.slot)) {
      target_block_root =
          blocks_.at(target_block_root).message.block.parent_root;
    }

    return Checkpoint{
        .root = target_block_root,
        .slot = blocks_.at(target_block_root).message.block.slot,
    };
  }

  AttestationData ForkChoiceStore::produceAttestationData(Slot slot) const {
    Checkpoint head_checkpoint{
        .root = head_,
        .slot = blocks_.at(head_).message.block.slot,
    };

    auto target_checkpoint = getAttestationTarget();

    return AttestationData{
        .slot = slot,
        .head = head_checkpoint,
        .target = target_checkpoint,
        .source = latest_justified_,
    };
  }

  outcome::result<SignedBlockWithAttestation>
  ForkChoiceStore::produceBlockWithSignatures(Slot slot,
                                              ValidatorIndex validator_index) {
    // Get parent block and state to build upon
    const auto &head_root = getHead();
    const auto &head_state = getState(head_root);

    // Validate proposer authorization for this slot
    if (not isProposer(validator_index, slot, head_state.validatorCount())) {
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
      // First advance state to target slot, then process the block
      auto post_state = head_state;
      BOOST_OUTCOME_TRY(stf_.processSlots(post_state, slot));
      BOOST_OUTCOME_TRY(stf_.processBlock(post_state, candidate_block));

      // Find new valid attestations matching post-state justification
      auto new_attestations = false;
      for (auto &signed_attestation :
           latest_known_attestations_ | std::views::values) {
        // Skip if target block is unknown in our store
        auto &data = signed_attestation.message.data;
        if (not blocks_.contains(data.head.root)) {
          continue;
        }

        // Skip if attestation source does not match post-state's latest
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

    // Create final block with all collected attestations
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
                      stf_.stateTransition(block, head_state, false));
    block.state_root = sszHash(state);
    block.setHash();

    auto proposer_attestation = produceAttestation(slot, validator_index);
    proposer_attestation.data.head = Checkpoint::from(block);

    // Sign proposer attestation
    auto payload = sszHash(proposer_attestation);
    crypto::xmss::XmssSignature signature = xmss_provider_->sign(
        validator_keys_manifest_->currentNodeXmssKeypair().private_key,
        slot,
        payload);
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
      Slot slot, ValidatorIndex validator_index) {
    return Attestation{
        .validator_id = validator_index,
        .data = produceAttestationData(slot),
    };
  }

  outcome::result<void> ForkChoiceStore::validateAttestation(
      const SignedAttestation &signed_attestation) {
    auto &data = signed_attestation.message.data;

    SL_TRACE(logger_,
             "Validating attestation for target {}, source {}",
             data.target,
             data.source);
    auto timer = metrics_->fc_attestation_validation_time_seconds()->timer();

    // Availability Check
    //
    // We cannot count a vote if we haven't seen the blocks involved.
    if (not blocks_.contains(data.source.root)) {
      return Error::INVALID_ATTESTATION;
    }
    if (not blocks_.contains(data.target.root)) {
      return Error::INVALID_ATTESTATION;
    }
    if (not blocks_.contains(data.head.root)) {
      return Error::INVALID_ATTESTATION;
    }

    // Topology Check
    //
    // History is linear and monotonic. Source must be older than Target.
    if (data.source.slot > data.target.slot) {
      return Error::INVALID_ATTESTATION;
    }

    // Consistency Check
    //
    // Validate checkpoint slots match block slots
    auto &source_block = blocks_.at(data.source.root).message.block;
    auto &target_block = blocks_.at(data.target.root).message.block;
    if (source_block.slot != data.source.slot) {
      return Error::INVALID_ATTESTATION;
    }
    if (target_block.slot != data.target.slot) {
      return Error::INVALID_ATTESTATION;
    }

    // Time Check
    //
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
    if (auto res = validateAttestation(signed_attestation); res.has_value()) {
      metrics_->fc_attestations_valid_total()->inc();
    } else {
      metrics_->fc_attestations_invalid_total()->inc();
      return res;
    }

    // Extract the validator index that produced this attestation.
    auto &validator_id = signed_attestation.message.validator_id;

    // Extract the attestation's slot:
    // - used to decide if this attestation is "newer" than a previous one.
    auto &attestation_slot = signed_attestation.message.data.slot;

    if (is_from_block) {
      // On-chain attestation processing
      //
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
      //
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
    //
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
    auto it = states_.find(block.parent_root);
    if (it == states_.end()) {
      SL_WARN(logger_, "Parent state not found for block");
      return false;
    }
    const auto &parent_state = it->second;
    const auto &validators = parent_state.validators;

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
      if (not xmss_provider_->verify(
              validator.pubkey, message, epoch, signature)) {
        SL_WARN(logger_,
                "Attestation signature verification failed for validator {}",
                validator_id);
        return false;
      }
    }
    return true;
  }

  outcome::result<void> ForkChoiceStore::onBlock(
      SignedBlockWithAttestation signed_block_with_attestation) {
    auto timer = metrics_->fc_block_processing_time_seconds()->timer();
    auto &block = signed_block_with_attestation.message.block;
    auto &proposer_attestation =
        signed_block_with_attestation.message.proposer_attestation;
    auto &signatures = signed_block_with_attestation.signature;

    block.setHash();
    auto block_hash = block.hash();
    // If the block is already known, ignore it
    if (blocks_.contains(block_hash)) {
      return outcome::success();
    }

    // Verify parent chain is available
    //
    // The parent state must exist before processing this block.
    // If missing, the node must sync the parent chain first.
    auto &parent_state = states_.at(block.parent_root);
    // at this point parent state should be available so node should sync
    // parent chain if not available before adding block to forkchoice

    auto valid_signatures =
        validateBlockSignatures(signed_block_with_attestation);
    if (not valid_signatures) {
      SL_WARN(logger_, "Invalid signatures for block {}", block.slotHash());
      return Error::INVALID_ATTESTATION;
    }

    // Get post state from STF (State Transition Function)
    BOOST_OUTCOME_TRY(auto post_state,
                      stf_.stateTransition(block, parent_state, true));

    // If post-state has a higher justified checkpoint, update it to the store.
    if (post_state.latest_justified.slot > latest_justified_.slot) {
      latest_justified_ = post_state.latest_justified;
    }

    // If post-state has a higher finalized checkpoint, update it to the store.
    if (post_state.latest_finalized.slot > latest_finalized_.slot) {
      latest_finalized_ = post_state.latest_finalized;
    }

    blocks_.emplace(block_hash, signed_block_with_attestation);
    states_.emplace(block_hash, std::move(post_state));

    // Process block body attestations
    //
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
    //
    // IMPORTANT: This must happen BEFORE processing proposer attestation
    // to prevent the proposer from gaining circular weight advantage.
    updateHead();

    // Process proposer attestation as if received via gossip
    //
    // The proposer casts their attestation in interval 1, after block
    // proposal. This attestation should:
    // 1. NOT affect this block's fork choice position (processed as "new")
    // 2. Be available for inclusion in future blocks
    // 3. Influence fork choice only after interval 3 (end of slot)
    BOOST_OUTCOME_TRY(onAttestation(
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

    auto validator_count = getState(head_).validatorCount();

    std::vector<std::variant<SignedAttestation, SignedBlockWithAttestation>>
        result{};
    while (time_ <= time_since_genesis) {
      Slot current_slot = time_ / INTERVALS_PER_SLOT;
      if (current_slot == 0) {
        // Skip actions for slot zero, which is the genesis slot
        time_ += 1;
        continue;
      }
      if (time_ % INTERVALS_PER_SLOT == 0) {
        // Slot start
        SL_INFO(logger_,
                "Slot {} started with time {}",
                current_slot,
                time_ * SECONDS_PER_INTERVAL);
        auto producer_index = current_slot % validator_count;
        auto is_producer =
            validator_registry_->currentValidatorIndices().contains(
                producer_index);
        if (is_producer) {
          acceptNewAttestations();

          auto res = produceBlockWithSignatures(current_slot, producer_index);
          if (!res.has_value()) {
            SL_ERROR(logger_,
                     "Failed to produce block for slot {}: {}",
                     current_slot,
                     res.error());
            time_ += 1;
            continue;
          }
          auto &new_signed_block = res.value();

          SL_INFO(logger_,
                  "Produced block {} with parent {} state {}",
                  new_signed_block.message.block.slotHash(),
                  new_signed_block.message.block.parent_root,
                  new_signed_block.message.block.state_root);
          result.emplace_back(std::move(new_signed_block));
        }
      } else if (time_ % INTERVALS_PER_SLOT == 1) {
        // Interval one actions
        auto head_root = getHead();
        auto head_slot = getBlockSlot(head_root);
        if (not head_slot.has_value()) {
          SL_ERROR(logger_, "Head block {} not found in store", head_root);
          time_ += 1;
          continue;
        }
        metrics_->fc_head_slot()->set(head_slot.value());
        Checkpoint head{.root = head_root, .slot = head_slot.value()};
        auto target = getAttestationTarget();
        SL_INFO(logger_,
                "For slot {}: head is {}, target is {}, source is {}",
                current_slot,
                head,
                target,
                latest_justified_);
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
          crypto::xmss::XmssSignature signature =
              xmss_provider_->sign(keypair.private_key, current_slot, payload);
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
          SL_INFO(logger_,
                  "Produced vote for target {}",
                  signed_attestation.message.data.target);
          result.emplace_back(std::move(signed_attestation));
        }
      } else if (time_ % INTERVALS_PER_SLOT == 2) {
        // Interval two actions
        SL_INFO(logger_,
                "Interval two of slot {} at time {}",
                current_slot,
                time_ * SECONDS_PER_INTERVAL);
        updateSafeTarget();
      } else if (time_ % INTERVALS_PER_SLOT == 3) {
        // Interval three actions
        SL_INFO(logger_,
                "Interval three of slot {} at time {}",
                current_slot,
                time_ * SECONDS_PER_INTERVAL);
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
    BOOST_ASSERT(not blocks_.empty());

    // If the starting point is not defined, choose the earliest known block.
    //
    // This ensures that the walk always has an anchor.
    auto anchor = start_root;
    if (anchor == kZeroHash or not blocks_.contains(anchor)) {
      anchor = std::min_element(blocks_.begin(),
                                blocks_.end(),
                                [](const auto &lhs, const auto &rhs) {
                                  return lhs.second.message.block.slot
                                       < rhs.second.message.block.slot;
                                })
                   ->first;
    }

    // Remember the slot of the anchor once and reuse it during the walk.
    //
    // This avoids repeated lookups inside the inner loop.
    const auto start_slot = blocks_.at(anchor).message.block.slot;

    // Prepare a table that will collect voting weight for each block.
    //
    // Each entry starts conceptually at zero and then accumulates
    // contributions.
    std::unordered_map<BlockHash, uint64_t> weights;
    auto get_weight = [&](const BlockHash &hash) {
      auto it = weights.find(hash);
      return it != weights.end() ? it->second : 0;
    };

    // For every vote, follow the chosen head upward through its ancestors.
    //
    // Each visited block accumulates one unit of weight from that validator.
    for (auto &attestation : attestations | std::views::values) {
      auto current = attestation.message.data.head.root;

      // Climb towards the anchor while staying inside the known tree.
      //
      // This naturally handles partial views and ongoing sync.
      while (blocks_.contains(current)
             and blocks_.at(current).message.block.slot > start_slot) {
        ++weights[current];
        current = blocks_.at(current).message.block.parent_root;
      }
    }

    // Build the adjacency tree (parent -> children).
    //
    // We use a map to avoid checking if keys exist.
    std::unordered_map<BlockHash, std::vector<BlockHash>> children_map;
    for (auto &[hash, block] : blocks_) {
      // 1. Structural check: skip blocks without parents (e.g., purely
      // genesis/orphans)
      if (block.message.block.parent_root == BlockHash{}) {
        continue;
      }

      // 2. Heuristic check: prune branches early if they lack sufficient weight
      if (min_score > 0 and get_weight(hash) < min_score) {
        continue;
      }

      children_map[block.message.block.parent_root].push_back(hash);
    }

    // Now perform the greedy walk.
    //
    // At each step, pick the child with the highest weight among the
    // candidates.
    auto head = anchor;

    // Descend the tree, choosing the heaviest branch at every fork.
    while (true) {
      auto it = children_map.find(head);
      if (it == children_map.end()) {
        return head;
      }
      auto &children = it->second;

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
      const GenesisConfig &genesis_config,
      qtils::SharedRef<clock::SystemClock> clock,
      qtils::SharedRef<log::LoggingSystem> logging_system,
      qtils::SharedRef<metrics::Metrics> metrics,
      qtils::SharedRef<ValidatorRegistry> validator_registry,
      qtils::SharedRef<app::ValidatorKeysManifest> validator_keys_manifest,
      qtils::SharedRef<crypto::xmss::XmssProvider> xmss_provider)
      : ForkChoiceStore{
            genesis_config.state,
            STF::genesisBlock(genesis_config.state),
            std::move(clock),
            std::move(logging_system),
            std::move(metrics),
            std::move(validator_registry),
            std::move(validator_keys_manifest),
            std::move(xmss_provider),
        } {}

  ForkChoiceStore::ForkChoiceStore(
      const State &anchor_state,
      const Block &anchor_block,
      qtils::SharedRef<clock::SystemClock> clock,
      qtils::SharedRef<log::LoggingSystem> logging_system,
      qtils::SharedRef<metrics::Metrics> metrics,
      qtils::SharedRef<ValidatorRegistry> validator_registry,
      qtils::SharedRef<app::ValidatorKeysManifest> validator_keys_manifest,
      qtils::SharedRef<crypto::xmss::XmssProvider> xmss_provider)
      : stf_(metrics),
        validator_registry_(validator_registry),
        validator_keys_manifest_(validator_keys_manifest),
        logger_(
            logging_system->getLogger("ForkChoiceStore", "fork_choice_store")),
        metrics_(std::move(metrics)),
        xmss_provider_(std::move(xmss_provider)) {
    BOOST_ASSERT(anchor_block.state_root == sszHash(anchor_state));
    anchor_block.setHash();
    auto anchor_root = anchor_block.hash();
    config_ = anchor_state.config;
    auto now_sec = clock->nowSec();
    time_ = now_sec > config_.genesis_time
              ? (now_sec - config_.genesis_time) / SECONDS_PER_INTERVAL
              : 0;
    head_ = anchor_root;
    safe_target_ = anchor_root;

    // TODO: ensure latest justified and finalized are set correctly
    latest_justified_ = Checkpoint::from(anchor_block);
    latest_finalized_ = Checkpoint::from(anchor_block);

    blocks_.emplace(anchor_root,
                    SignedBlockWithAttestation{
                        .message = {.block = std::move(anchor_block)},
                    });
    SL_INFO(
        logger_, "Anchor block {} at slot {}", anchor_root, anchor_block.slot);
    states_.emplace(anchor_root, anchor_state);
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
      BlockHash head,
      BlockHash safe_target,
      Checkpoint latest_justified,
      Checkpoint latest_finalized,
      Blocks blocks,
      std::unordered_map<BlockHash, State> states,
      SignedAttestations latest_known_attestations,
      SignedAttestations latest_new_attestations,
      ValidatorIndex validator_index,
      qtils::SharedRef<ValidatorRegistry> validator_registry,
      qtils::SharedRef<app::ValidatorKeysManifest> validator_keys_manifest,
      qtils::SharedRef<crypto::xmss::XmssProvider> xmss_provider)
      : stf_(metrics),
        time_(now_sec / SECONDS_PER_INTERVAL),
        logger_(
            logging_system->getLogger("ForkChoiceStore", "fork_choice_store")),
        config_(config),
        head_(head),
        safe_target_(safe_target),
        latest_justified_(latest_justified),
        latest_finalized_(latest_finalized),
        blocks_(std::move(blocks)),
        states_(std::move(states)),
        latest_known_attestations_(std::move(latest_known_attestations)),
        latest_new_attestations_(std::move(latest_new_attestations)),
        metrics_(std::move(metrics)),
        validator_registry_(std::move(validator_registry)),
        validator_keys_manifest_(std::move(validator_keys_manifest)),
        xmss_provider_(std::move(xmss_provider)) {}
}  // namespace lean
