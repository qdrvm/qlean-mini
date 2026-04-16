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

#include "app/chain_spec.hpp"
#include "app/configuration.hpp"
#include "app/validator_keys_manifest.hpp"
#include "blockchain/genesis_config.hpp"
#include "blockchain/is_proposer.hpp"
#include "blockchain/validator_registry.hpp"
#include "blockchain/validator_subnet.hpp"
#include "crypto/xmss/xmss_provider.hpp"
#include "impl/block_tree_impl.hpp"
#include "is_justifiable_slot.hpp"
#include "metrics/impl/metrics_impl.hpp"
#include "types/aggregated_attestations.hpp"
#include "types/attestation.hpp"
#include "types/fork_choice_api_json.hpp"
#include "types/signed_block.hpp"
#include "utils/ceil_div.hpp"
#include "utils/lru_cache.hpp"
#include "utils/retain_if.hpp"

namespace lean {
  inline ValidatorIndex getValidatorId(
      const log::Logger &logger, const ValidatorRegistry &validator_registry) {
    auto &indices = validator_registry.currentValidatorIndices();
    if (indices.size() != 1) {
      SL_FATAL(logger, "multiple validators on same node are not supported");
    }
    return *indices.begin();
  }

  ForkChoiceStore::ForkChoiceStore(
      qtils::SharedRef<AnchorState> anchor_state,
      qtils::SharedRef<AnchorBlock> anchor_block,
      qtils::SharedRef<clock::SystemClock> clock,
      qtils::SharedRef<log::LoggingSystem> logging_system,
      qtils::SharedRef<metrics::Metrics> metrics,
      qtils::SharedRef<app::Configuration> app_config,
      qtils::SharedRef<ValidatorRegistry> validator_registry,
      qtils::SharedRef<app::ChainSpec> chain_spec,
      qtils::SharedRef<app::ValidatorKeysManifest> validator_keys_manifest,
      qtils::SharedRef<crypto::xmss::XmssProvider> xmss_provider,
      qtils::SharedRef<blockchain::BlockTree> block_tree,
      qtils::SharedRef<blockchain::BlockStorage> block_storage)
      : logger_(logging_system->getLogger("ForkChoice", "fork_choice")),
        metrics_(std::move(metrics)),
        xmss_provider_(std::move(xmss_provider)),
        block_tree_(std::move(block_tree)),
        block_storage_(std::move(block_storage)),
        stf_(std::move(logging_system), block_tree_, metrics_),
        config_(anchor_state->config),
        validator_registry_(std::move(validator_registry)),
        validator_keys_manifest_(std::move(validator_keys_manifest)),
        validator_id_{getValidatorId(logger_, *validator_registry_)},
        is_aggregator_{chain_spec->isAggregator()},
        subnet_count_{app_config->cliSubnetCount()} {
    metrics_->lean_is_aggregator()->set(is_aggregator_ ? 1 : 0);
    metrics_->stf_latest_justified_slot()->set(
        block_tree_->getLatestJustified().slot);
    metrics_->stf_latest_finalized_slot()->set(
        block_tree_->lastFinalized().slot);

    SL_TRACE(logger_, "Initialise fork-choice");

    for (auto xmss_pubkey : validator_keys_manifest_->getAllXmssPubkeys()) {
      SL_INFO(logger_, "Validator pubkey: {}", xmss_pubkey.toHex());
    }

    BOOST_ASSERT(anchor_block->state_root == sszHash(*anchor_state));
    SL_TRACE(logger_, "Anchor block: {}", anchor_block->index());
    SL_TRACE(logger_, "Anchor state: {}", anchor_block->state_root);

    auto now_ms = clock->nowMsec();
    time_ =
        Interval::fromTime(now_ms, config_).value_or(Interval::fromSlot(0, 0));

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

    for (auto &&[slot, hash] : std::views::zip(
             std::views::iota(
                 Slot{0}, Slot{anchor_state->historical_block_hashes.size()}),
             anchor_state->historical_block_hashes)) {
      if (hash == kZeroHash) {
        continue;
      }
      anchor_block_slots_.emplace(hash, slot);
    }
  }

  // Test constructor implementation
  ForkChoiceStore::ForkChoiceStore(
      Interval time,
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
      qtils::SharedRef<blockchain::BlockStorage> block_storage,
      bool is_aggregator,
      uint64_t subnet_count)
      : logger_(logging_system->getLogger("ForkChoice", "fork_choice")),
        metrics_(std::move(metrics)),
        xmss_provider_(std::move(xmss_provider)),
        block_tree_(std::move(block_tree)),
        block_storage_(std::move(block_storage)),
        stf_(std::move(logging_system), block_tree_, metrics_),
        time_{time},
        config_(config),
        head_(head),
        safe_target_(safe_target),
        latest_known_attestations_(std::move(latest_known_attestations)),
        latest_new_attestations_(std::move(latest_new_attestations)),
        validator_registry_(std::move(validator_registry)),
        validator_keys_manifest_(std::move(validator_keys_manifest)),
        validator_id_{getValidatorId(logger_, *validator_registry_)},
        is_aggregator_{is_aggregator},
        subnet_count_{subnet_count} {}

  void ForkChoiceStore::dontPropose() {
    dont_propose_ = true;
  }

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

    metrics_->fc_safe_target_slot()->set(safe_target_.slot);
    return outcome::success();
  }

  outcome::result<void> ForkChoiceStore::updateHead() {
    SL_TRACE(logger_, "Update head");
    // Run LMD-GHOST fork choice algorithm
    //
    // Selects canonical head by walking the tree from the justified root,
    // choosing the heaviest child at each fork based on attestation weights.
    OUTCOME_TRY(lmd_ghost_head_root,
                computeLmdGhostHead(block_tree_->getLatestJustified().root,
                                    latest_known_attestations_,
                                    0));

    OUTCOME_TRY(lmd_ghost_head_slot, getBlockSlot(lmd_ghost_head_root));
    Checkpoint lmd_ghost_head = {.root = lmd_ghost_head_root,
                                 .slot = lmd_ghost_head_slot};

    // Reorg detection
    if (head_.root != lmd_ghost_head.root) {
      Checkpoint a = head_;
      Checkpoint b = lmd_ghost_head;

      size_t da = 0;
      size_t db = 0;

      auto lift_one = [&](Checkpoint &cp, size_t &d) -> outcome::result<void> {
        OUTCOME_TRY(header, block_tree_->getBlockHeader(cp.root));
        auto &parent_root = header.parent_root;
        OUTCOME_TRY(parent_slot, block_tree_->getSlotByHash(parent_root));
        cp = {.root = parent_root, .slot = parent_slot};
        ++d;
        return outcome::success();
      };

      while (a.root != b.root) {
        if (a.slot > b.slot) {
          OUTCOME_TRY(lift_one(a, da));
          continue;
        }
        if (b.slot > a.slot) {
          OUTCOME_TRY(lift_one(b, db));
          continue;
        }
        OUTCOME_TRY(lift_one(a, da));
        OUTCOME_TRY(lift_one(b, db));
      }

      const bool lmd_is_descendant_of_head = (a.root == head_.root);
      if (not lmd_is_descendant_of_head) {
        metrics_->fc_fork_choice_reorgs_total()->inc();
        metrics_->fc_fork_choice_reorg_depth()->observe(
            static_cast<double>(std::max(da, db)));
      }
    }

    head_ = lmd_ghost_head;
    SL_TRACE(logger_, "Head was set to {}", head_);

    metrics_->fc_head_slot()->set(head_.slot);
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
    return time_.slot();
  }

  Checkpoint ForkChoiceStore::getHead() const {
    return head_;
  }

  outcome::result<std::shared_ptr<const State>> ForkChoiceStore::getState(
      const BlockHash &block_hash) const {
    SL_TRACE(logger_, "Getting state for block {:xx}", block_hash);
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
    auto it = anchor_block_slots_.find(block_hash);
    if (it != anchor_block_slots_.end()) {
      return it->second;
    }
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

  Checkpoint ForkChoiceStore::getAttestationTarget(
      const Checkpoint &justified,
      const Checkpoint &head,
      std::optional<BlockHash> head_parent_hash) const {
    auto get_slot_and_parent =
        [&](const BlockHash &hash) -> std::pair<Slot, BlockHash> {
      if (head_parent_hash.has_value() and hash == head.root) {
        return {head.slot, head_parent_hash.value()};
      }
      auto header_res = block_tree_->getBlockHeader(hash);
      if (header_res.has_error()) {
        SL_FATAL(logger_,
                 "Failed getting header of head or some it's ancestor: {}",
                 header_res.error());
      }
      auto &header = header_res.value();
      return {header.slot, header.parent_root};
    };

    // Start from head as target-candidate
    auto target_block_root = head.root;

    // If there is no very recent safe target, then vote for the k'th ancestor
    // of the head
    const auto safe_target_slot = safe_target_.slot;

    // Ensure the attestation target is not older than the latest justified
    // block, as it would violate protocol rules and fail validation.
    auto latest_justified_slot = justified.slot;
    auto lookback_limit = std::max(safe_target_slot, latest_justified_slot);

    for (auto i = 0; i < JUSTIFICATION_LOOKBACK_SLOTS; ++i) {
      auto [target_slot, target_parent] =
          get_slot_and_parent(target_block_root);
      if (target_slot > lookback_limit) {
        target_block_root = target_parent;
      } else {
        break;
      }
    }

    // If the latest finalized slot is very far back, then only some slots are
    // valid to justify, make sure the target is one of those
    auto latest_finalized_slot = block_tree_->lastFinalized().slot;
    while (true) {
      auto [target_slot, target_parent] =
          get_slot_and_parent(target_block_root);
      if (isJustifiableSlot(latest_finalized_slot, target_slot)) {
        break;
      }
      target_block_root = target_parent;
    }

    return Checkpoint{
        .root = target_block_root,
        .slot = get_slot_and_parent(target_block_root).first,
    };
  }

  Attestation ForkChoiceStore::produceAttestation(
      Slot slot,
      ValidatorIndex validator_index,
      const Checkpoint &justified,
      const Checkpoint &head,
      std::optional<BlockHash> head_parent_hash) const {
    auto target_checkpoint =
        getAttestationTarget(justified, head, head_parent_hash);
    return Attestation{
        .validator_id = validator_index,
        .data =
            {
                .slot = slot,
                .head = head,
                .target = target_checkpoint,
                .source = justified,
            },
    };
  }

  outcome::result<SignedBlock> ForkChoiceStore::produceBlockWithSignatures(
      Slot slot, ValidatorIndex proposer_index) {
    // Get parent block and state to build upon
    auto head_root = head_.root;
    OUTCOME_TRY(head_state, getState(head_root));

    // Validate proposer authorization for this slot
    if (not isProposer(proposer_index, slot, head_state->validatorCount())) {
      return Error::INVALID_PROPOSER;
    }

    auto keypair = validator_keys_manifest_->getKeypair(
        head_state->validators.data().at(proposer_index).proposal_pubkey);
    if (not keypair.has_value()) {
      return Error::NO_KEYPAIR;
    }

    BOOST_OUTCOME_TRY(auto aggregated,
                      getProposalAttestations(slot, proposer_index, head_root));

    // Create the final block with all collected attestations
    Block block{
        .slot = slot,
        .proposer_index = proposer_index,
        .parent_root = head_root,
        // Will be updated with computed hash
        .state_root = {},
        .body = {.attestations = aggregated.first},
    };
    // Apply state transition to get final post-state and compute state root
    BOOST_OUTCOME_TRY(auto state,
                      stf_.stateTransition(block, *head_state, false));
    block.state_root = sszHash(state);
    block.setHash();

    auto proposer_attestation = produceAttestation(slot,
                                                   proposer_index,
                                                   state.latest_justified,
                                                   Checkpoint::from(block),
                                                   block.parent_root);

    // Sign proposer attestation
    auto payload = sszHash(block);
    crypto::xmss::XmssSignature proposer_signature =
        xmss_provider_->sign(keypair->private_key, slot, payload);
    metrics_->lean_pq_sig_attestation_signatures_total()->inc();
    SignedBlock signed_block{
        .block = block,
        .signature =
            {
                .attestation_signatures = aggregated.second,
                .proposer_signature = proposer_signature,
            },
    };
    BOOST_OUTCOME_TRY(onBlock(signed_block));

    return signed_block;
  }

  outcome::result<std::pair<AggregatedAttestations, AttestationSignatures>>
  ForkChoiceStore::getProposalAttestations(Slot slot,
                                           ValidatorIndex proposer_index,
                                           BlockHash parent_root) {
    OUTCOME_TRY(head_state, getState(parent_root));
    struct SourceThenTarget {
      static auto tie(const AttestationData &v) {
        return std::tie(v.source.slot,
                        v.source.root,
                        v.target.slot,
                        v.target.root,
                        v.head.slot,
                        v.head.root,
                        v.slot);
      }
      bool operator()(const AttestationData &l,
                      const AttestationData &r) const {
        return tie(l) < tie(r);
      }
    };
    std::set<AttestationData, SourceThenTarget> sorted_data;
    for (auto &data : latest_known_attestations_ | std::views::values) {
      if (not block_tree_->has(data.head.root)) {
        continue;
      }
      sorted_data.emplace(data);
    }
    AggregatedAttestations aggregated_attestations;
    AttestationSignatures aggregated_proofs;
    auto expected_source = head_state->latest_justified;
    for (auto &data : sorted_data) {
      if (data.source != expected_source) {
        continue;
      }
      auto attestations_it = attestations_by_data_.find(sszHash(data));
      if (attestations_it == attestations_by_data_.end()) {
        break;
      }
      auto &attestations = attestations_it->second;
      // TODO(zeam): producer may aggregate
      if (attestations.proofs.empty()) {
        break;
      }

      for (auto &proof : attestations.proofs) {
        aggregated_attestations.push_back({
            .aggregation_bits = proof.participants,
            .data = data,
        });
        aggregated_proofs.push_back(proof);
      }

      auto post_state = *head_state;
      BOOST_OUTCOME_TRY(stf_.processSlots(post_state, slot));
      BOOST_OUTCOME_TRY(stf_.processBlock(
          post_state,
          {
              .slot = slot,
              .proposer_index = proposer_index,
              .parent_root = parent_root,
              .state_root = {},
              .body = {.attestations = aggregated_attestations},
          }));
      expected_source = post_state.latest_justified;
    }
    return std::make_pair(aggregated_attestations, aggregated_proofs);
  }

  outcome::result<void> ForkChoiceStore::validateAttestation(
      const Attestation &attestation) {
    auto has_block = [&](const BlockHash &hash) {
      return anchor_block_slots_.contains(hash) or block_tree_->has(hash);
    };
    auto &data = attestation.data;

    SL_TRACE(logger_,
             "Validating attestation for target={}, source={}",
             data.target,
             data.source);
    auto timer = metrics_->fc_attestation_validation_time()->timer();

    // Availability Check

    // We cannot count a vote if we haven't seen the blocks involved.
    if (not has_block(data.source.root)) {
      return Error::CANT_VALIDATE_ATTESTATION_SOURCE_NOT_FOUND;
    }
    if (not has_block(data.target.root)) {
      return Error::CANT_VALIDATE_ATTESTATION_TARGET_NOT_FOUND;
    }
    if (not has_block(data.head.root)) {
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
        .data = signed_attestation.data,
    };
    BOOST_OUTCOME_TRY(validateAttestation(attestation));
    OUTCOME_TRY(state, getState(signed_attestation.data.target.root));
    if (signed_attestation.validator_id >= state->validators.size()) {
      return Error::INVALID_ATTESTATION;
    }
    auto payload = attestationPayload(signed_attestation.data);
    auto signature_valid = xmss_provider_->verify(
        state->validators[signed_attestation.validator_id].attestation_pubkey,
        payload,
        signed_attestation.data.slot,
        signed_attestation.signature);
    updateMetricAttestationSignature(signature_valid);
    if (not signature_valid) {
      return Error::INVALID_ATTESTATION;
    }
    if (is_aggregator_
        and validatorSubnet(signed_attestation.validator_id, subnet_count_)
                == validatorSubnet(validator_id_, subnet_count_)) {
      addSignatureToAggregate(signed_attestation.data,
                              signed_attestation.validator_id,
                              signed_attestation.signature);
    }
    return outcome::success();
  }

  outcome::result<void> ForkChoiceStore::onGossipAggregatedAttestation(
      const SignedAggregatedAttestation &signed_aggregated_attestation) {
    BOOST_OUTCOME_TRY(auto state,
                      getState(signed_aggregated_attestation.data.target.root));
    if (not validateAggregatedSignature(*state,
                                        signed_aggregated_attestation.data,
                                        signed_aggregated_attestation.proof)) {
      SL_WARN(logger_,
              "Signature verification failed for gossip aggregated attestation "
              "slot={} participants=[{}]",
              signed_aggregated_attestation.data.slot,
              fmt::join(signed_aggregated_attestation.proof.participants.iter(),
                        " "));
      return outcome::success();
    }
    return onAggregatedAttestation(signed_aggregated_attestation, false);
  }

  outcome::result<void> ForkChoiceStore::onAggregatedAttestation(
      const SignedAggregatedAttestation &signed_aggregated_attestation,
      bool is_from_block) {
    for (auto &&validator_id :
         signed_aggregated_attestation.proof.participants.iter()) {
      // Store the aggregated signature payload against (validator_id,
      // data_root) This is a list because the same (validator_id, data) can
      // appear in multiple aggregated attestations, especially when we have
      // aggregator roles. This list can be recursively aggregated by the
      // block proposer.
      addProofToAggregate(signed_aggregated_attestation);

      // Import the attestation data into forkchoice for latest votes
      BOOST_OUTCOME_TRY(onAttestation(
          Attestation{
              .validator_id = validator_id,
              .data = signed_aggregated_attestation.data,
          },
          is_from_block));
    }
    return outcome::success();
  }

  outcome::result<void> ForkChoiceStore::onAttestation(
      const Attestation &attestation, bool is_from_block) {
    auto timer = metrics_->fc_attestation_validation_time()->timer();

    // First, ensure the attestation is structurally and temporally valid.

    // Extract node id
    auto node_id_opt =
        validator_registry_->nodeIdByIndex(attestation.validator_id);
    if (not node_id_opt.has_value()) {
      SL_WARN(logger_,
              "Received attestation from unknown validator index {}",
              attestation.validator_id);
    }

    if (auto res = validateAttestation(attestation); res.has_value()) {
      metrics_->fc_attestations_valid_total()->inc();
      SL_DEBUG(logger_,
               "⚙️ Processing valid attestation from validator {} for "
               "target={}, source={}",
               node_id_opt ? node_id_opt.value() : "unknown",
               attestation.data.target,
               attestation.data.source);
    } else {
      metrics_->fc_attestations_invalid_total()->inc();
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
      const SignedBlock &signed_block) const {
    // Unpack the signed block components
    const auto &block = signed_block.block;
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
    auto &parent_state = *parent_state_res.value();
    const auto &validators = parent_state.validators;

    for (auto &&[aggregated_attestation, aggregated_signature] :
         std::views::zip(aggregated_attestations, attestation_signatures)) {
      if (not validateAggregatedSignature(parent_state,
                                          aggregated_attestation.data,
                                          aggregated_signature)) {
        return false;
      }
    }
    auto payload = sszHash(block);

    bool verify_result = xmss_provider_->verify(
        validators.data().at(block.proposer_index).proposal_pubkey,
        payload,
        block.slot,
        signed_block.signature.proposer_signature);
    updateMetricAttestationSignature(verify_result);

    if (not verify_result) {
      SL_WARN(logger_,
              "Proposer signature verification failed for validator {}",
              block.proposer_index);
      return false;
    }
    SL_TRACE(
        logger_, "All block signatures are valid in block {}", block.index());
    return true;
  }

  bool ForkChoiceStore::validateAggregatedSignature(
      const State &state,
      const AttestationData &attestation,
      const AggregatedSignatureProof &signature) const {
    std::vector<crypto::xmss::XmssPublicKey> public_keys;
    for (auto &&validator_id : signature.participants.iter()) {
      if (validator_id >= state.validators.size()) {
        SL_WARN(logger_, "Validator index {} out of range", validator_id);
        return false;
      }
      public_keys.emplace_back(
          state.validators.data().at(validator_id).attestation_pubkey);
    }
    auto message = attestationPayload(attestation);
    Epoch epoch = attestation.slot;
    bool verify_result = xmss_provider_->verifyAggregatedSignatures(
        public_keys, epoch, message, signature.proof_data.data());
    if (not verify_result) {
      SL_WARN(logger_,
              "Aggregated signature verification failed for validators [{}]",
              fmt::join(signature.participants.iter(), " "));
      return false;
    }
    return true;
  }

  outcome::result<void> ForkChoiceStore::onBlock(SignedBlock signed_block) {
    auto &block = signed_block.block;
    block.setHash();
    auto block_hash = block.hash();

    // If the block is already known, ignore it
    if (block_tree_->has(block_hash)) {
      return outcome::success();
    }

    auto &signatures = signed_block.signature;

    auto timer = metrics_->fc_block_processing_time()->timer();

    // Verify parent-chain is available

    // The parent state must exist before processing this block.
    // If missing, the node must sync the parent chain first.

    OUTCOME_TRY(parent_state, getState(block.parent_root));

    // at this point parent state should be available so node should sync
    // parent-chain if not available before adding block to forkchoice

    auto valid_signatures = validateBlockSignatures(signed_block);
    if (not valid_signatures) {
      SL_WARN(logger_, "Invalid signatures for block {}", block.index());
      return Error::INVALID_ATTESTATION;
    }

    // Get post-state from STF (State Transition Function)
    BOOST_OUTCOME_TRY(auto post_state,
                      stf_.stateTransition(block, *parent_state, true));

    // Add block
    SL_TRACE(logger_, "Adding block {} into block tree", block.index());
    OUTCOME_TRY(block_tree_->addBlock(signed_block));

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
      metrics_->stf_latest_justified_slot()->set(
          post_state.latest_justified.slot);
    }

    // If post-state has a higher finalized checkpoint, update it to the store.
    if (post_state.latest_finalized.slot > block_tree_->lastFinalized().slot) {
      OUTCOME_TRY(block_tree_->finalize(post_state.latest_finalized.root));
      SL_INFO(logger_, "🔒 Finalized block: {}", post_state.latest_finalized);
      metrics_->stf_latest_finalized_slot()->set(
          post_state.latest_finalized.slot);

      prune(post_state.latest_finalized.slot);
    }

    // Cache state
    states_.put(block_hash, post_state);

    // Process block body attestations
    auto &aggregated_attestations = signed_block.block.body.attestations;
    auto &attestation_signatures =
        signed_block.signature.attestation_signatures;
    if (attestation_signatures.size() != aggregated_attestations.size()) {
      return Error::SIGNATURE_COUNT_MISMATCH;
    }
    for (auto &&[aggregated_attestation, aggregated_signature] :
         std::views::zip(aggregated_attestations, attestation_signatures)) {
      BOOST_OUTCOME_TRY(onAggregatedAttestation(
          {.data = aggregated_attestation.data, .proof = aggregated_signature},
          true));
    }

    // Update fork-choice head based on new block and attestations

    // IMPORTANT: This must happen BEFORE processing proposer attestation
    // to prevent the proposer from gaining circular weight advantage.
    OUTCOME_TRY(updateHead());

    return outcome::success();
  }

  std::vector<ForkChoiceStore::OnTickAction> ForkChoiceStore::onTick(
      std::chrono::milliseconds now) {
    auto now_interval = Interval::fromTime(now, config_);
    if (not now_interval.has_value()) {
      SL_WARN(logger_, "Can't tick before genesis");
      return {};
    }

    auto head_state_res = getState(head_.root);
    if (head_state_res.has_error()) {
      SL_FATAL(logger_,
               "Fatal error: Failed getting state of head ({}): {}",
               head_state_res.error());
    }
    auto head_state = head_state_res.value();

    auto validator_count = head_state->validatorCount();

    std::vector<OnTickAction> result{};
    while (time_.interval <= now_interval->interval) {
      Slot current_slot = time_.slot();
      metrics_->fc_current_slot()->set(current_slot);
      if (time_.phase() == 0) {
        [[unlikely]] if (current_slot == 0) {
          // Skip propose for slot zero, which is the genesis slot
          time_.interval += 1;
          continue;
        }
        // Slot start
        SL_DEBUG(logger_, "Slot {} started", current_slot);
        auto producer_index = current_slot % validator_count;
        auto is_producer =
            validator_registry_->currentValidatorIndices().contains(
                producer_index);

        if (is_producer and not dont_propose_) {
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
            time_.interval += 1;
            continue;
          }
          auto &produced_block = res.value();

          SL_TRACE(logger_,
                   "👷 Produced block {} with parent {} and state {}",
                   produced_block.block.index(),
                   produced_block.block.parent_root,
                   produced_block.block.state_root);
          result.emplace_back(std::move(produced_block));


          head_state_res = getState(head_.root);
          if (head_state_res.has_error()) {
            SL_FATAL(logger_,
                     "Fatal error: Failed getting state of head ({}): {}",
                     head_state_res.error());
          }
          head_state = head_state_res.value();
        } else {
          SL_TRACE(logger_,
                   "Interval 0 of slot {}: node isn't producer - skip",
                   current_slot);
        }

      } else if (time_.phase() == 1) {
        SL_TRACE(logger_, "Interval 1 of slot {}", current_slot);

        // Ensure the head is updated before voting
        auto ana_res = acceptNewAttestations();
        if (ana_res.has_error()) {
          SL_WARN(logger_,
                  "Failed to accept new attestations: {}",
                  ana_res.error());
        }

        Checkpoint head = head_;
        auto target =
            getAttestationTarget(getLatestJustified(), head_, std::nullopt);
        auto source = block_tree_->getLatestJustified();
        SL_INFO(logger_, "🔷 Head={}", head);
        SL_INFO(logger_, "🎯 Target={}", target);
        SL_INFO(logger_, "📌 Source={}", source);

        if (source.slot > target.slot) {
          SL_WARN(logger_,
                  "Attestation source slot {} is not less than target slot {}",
                  source.slot,
                  target.slot);
          time_.interval += 1;
          continue;
        }

        for (auto validator_index :
             validator_registry_->currentValidatorIndices()) {
          if (dont_propose_) {
            continue;
          }
          auto keypair =
              validator_keys_manifest_->getKeypair(head_state->validators.data()
                                                       .at(validator_index)
                                                       .attestation_pubkey);
          if (not keypair.has_value()) {
            continue;
          }
          auto attestation = produceAttestation(current_slot,
                                                validator_index,
                                                getLatestJustified(),
                                                head_,
                                                std::nullopt);
          // sign attestation
          auto payload = attestationPayload(attestation.data);
          crypto::xmss::XmssSignature signature =
              xmss_provider_->sign(keypair->private_key, current_slot, payload);
          metrics_->lean_pq_sig_attestation_signatures_total()->inc();
          auto signed_attestation =
              SignedAttestation::from(attestation, signature);

          // Dispatching send signed vote-only broadcasts to other peers.
          // Current peer should process attestation directly
          auto res = onGossipAttestation(signed_attestation);
          if (not res.has_value()) {
            SL_ERROR(logger_,
                     "Failed to process attestation for slot {}: {}",
                     current_slot,
                     res.error());
            continue;
          }
          SL_DEBUG(logger_,
                   "Produced vote for target={}",
                   signed_attestation.data.target);
          result.emplace_back(signed_attestation);
        }

      } else if (time_.phase() == 2) {
        SL_TRACE(logger_, "Interval 2 of slot {}: aggregate", current_slot);
        if (is_aggregator_) {
          auto aggregated_attestations = aggregateSignatures();
          for (auto &aggregated_attestation : aggregated_attestations) {
            auto res = onGossipAggregatedAttestation(aggregated_attestation);
            if (not res.has_value()) {
              SL_WARN(
                  logger_, "failed to import own aggregation: {}", res.error());
              continue;
            }
            result.emplace_back(aggregated_attestation);
          }
        }
      } else if (time_.phase() == 3) {
        SL_TRACE(logger_,
                 "Interval 3 of slot {}: update safe-target ",
                 current_slot);

        auto res = updateSafeTarget();
        if (res.has_error()) {
          SL_WARN(logger_, "Failed to update safe-target: {}", res.error());
        }

      } else if (time_.phase() == 4) {
        SL_TRACE(logger_,
                 "Interval 4 of slot {}: accepting new attestations",
                 current_slot);

        auto ana_res = acceptNewAttestations();
        if (ana_res.has_error()) {
          SL_WARN(logger_,
                  "Failed to accept new attestations: {}",
                  ana_res.error());
        }
      }
      time_.interval += 1;
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

  outcome::result<ForkChoiceApiJson> ForkChoiceStore::apiForkChoice() const {
    auto finalized = getLatestFinalized();
    auto head = getHead().root;
    BOOST_OUTCOME_TRY(auto head_state, getState(head));

    std::unordered_map<BlockHash, ForkChoiceNodeApiJson> nodes;
    auto block_queue = block_tree_->getLeaves();
    while (not block_queue.empty()) {
      auto hash = block_queue.back();
      block_queue.pop_back();
      BOOST_OUTCOME_TRY(auto header, block_tree_->tryGetBlockHeader(hash));
      if (not header.has_value()) {
        continue;
      }
      if (header->slot < finalized.slot) {
        continue;
      }
      nodes.emplace(hash,
                    ForkChoiceNodeApiJson{
                        .root = hash,
                        .slot = header->slot,
                        .parent_root = header->parent_root,
                        .proposer_index = header->proposer_index,
                        .weight = 0,
                    });
      if (header->slot > finalized.slot) {
        block_queue.emplace_back(header->parent_root);
      }
    }

    for (auto &attestation : latest_known_attestations_ | std::views::values) {
      auto hash = attestation.head.root;
      while (true) {
        auto node_it = nodes.find(hash);
        if (node_it == nodes.end()) {
          break;
        }
        auto &node = node_it->second;
        ++node.weight;
        hash = node.parent_root;
      }
    }

    ForkChoiceApiJson result{
        .head = head,
        .justified = getLatestJustified(),
        .finalized = finalized,
        .safe_target = getSafeTarget().root,
        .validator_count = head_state->validatorCount(),
    };
    for (auto &node : nodes | std::views::values) {
      result.nodes.emplace_back(std::move(node));
    }
    return result;
  }

  void ForkChoiceStore::addSignatureToAggregate(const AttestationData &data,
                                                ValidatorIndex validator_index,
                                                const Signature &signature) {
    if (data.target.slot < getLatestFinalized().slot) {
      return;
    }
    auto &attestations = attestationsByData(data);
    for (auto &proof : attestations.proofs) {
      if (proof.participants.contains(validator_index)) {
        return;
      }
    }
    attestations.signatures[validator_index] = signature;
    updateMetricGossipSignatures();
  }

  void ForkChoiceStore::addProofToAggregate(
      const SignedAggregatedAttestation &signed_aggregated_attestation) {
    auto &data = signed_aggregated_attestation.data;
    if (data.target.slot < getLatestFinalized().slot) {
      return;
    }
    auto &attestations = attestationsByData(data);
    retain_if(attestations.proofs, [&](const AggregatedSignatureProof &proof) {
      return std::ranges::any_of(
                 proof.participants.iter(),
                 [&](ValidatorIndex i) {
                   return not signed_aggregated_attestation.proof.participants
                                  .contains(i);
                 })
          or std::ranges::all_of(
                 signed_aggregated_attestation.proof.participants.iter(),
                 [&](ValidatorIndex i) {
                   return proof.participants.contains(i);
                 });
    });
    AggregationBits existing_bits;
    for (auto &proof : attestations.proofs) {
      for (auto &&i : proof.participants.iter()) {
        existing_bits.add(i);
      }
    }
    if (std::ranges::all_of(
            signed_aggregated_attestation.proof.participants.iter(),
            [&](ValidatorIndex i) { return existing_bits.contains(i); })) {
      return;
    }
    attestations.proofs.emplace_back(signed_aggregated_attestation.proof);
    for (auto &&validator_index :
         signed_aggregated_attestation.proof.participants.iter()) {
      attestations.signatures.erase(validator_index);
    }
    updateMetricGossipSignatures();
  }

  ForkChoiceStore::AttestationsByData &ForkChoiceStore::attestationsByData(
      const AttestationData &data) {
    auto key = sszHash(data);
    auto it = attestations_by_data_.find(key);
    if (it == attestations_by_data_.end()) {
      it = attestations_by_data_.emplace(key, AttestationsByData{.data = data})
               .first;
    }
    return it->second;
  }

  std::vector<SignedAggregatedAttestation>
  ForkChoiceStore::aggregateSignatures() {
    auto timer =
        metrics_->lean_committee_signatures_aggregation_time_seconds()->timer();

    std::vector<SignedAggregatedAttestation> aggregated_attestations;
    for (auto &attestations : attestations_by_data_ | std::views::values) {
      if (attestations.signatures.empty() and attestations.proofs.size() <= 1) {
        continue;
      }
      auto state_res = getState(attestations.data.target.root);
      if (not state_res.has_value()) {
        continue;
      }
      auto &state = *state_res.value();
      std::vector<std::vector<crypto::xmss::XmssPublicKey>> child_public_keys;
      std::vector<crypto::xmss::XmssAggregatedSignature> child_proofs;
      std::vector<crypto::xmss::XmssPublicKey> public_keys;
      std::vector<Signature> signatures;
      AggregationBits participants;
      for (auto &[validator_id, signature] : attestations.signatures) {
        public_keys.emplace_back(
            state.validators.data().at(validator_id).attestation_pubkey);
        signatures.emplace_back(signature);
        participants.add(validator_id);
      }
      for (auto &proof : attestations.proofs) {
        std::vector<crypto::xmss::XmssPublicKey> public_keys;
        for (auto &&validator_id : proof.participants.iter()) {
          public_keys.emplace_back(
              state.validators.data().at(validator_id).attestation_pubkey);
          participants.add(validator_id);
        }
        child_public_keys.emplace_back(std::move(public_keys));
        child_proofs.emplace_back(proof.proof_data);
      }
      auto payload = attestationPayload(attestations.data);
      auto aggregated_signature =
          xmss_provider_->aggregateSignatures(child_public_keys,
                                              child_proofs,
                                              public_keys,
                                              signatures,
                                              attestations.data.slot,
                                              payload);
      AggregatedSignatureProof proof{
          .participants = participants,
          .proof_data = aggregated_signature,
      };
      aggregated_attestations.emplace_back(SignedAggregatedAttestation{
          .data = attestations.data,
          .proof = proof,
      });
      attestations.signatures.clear();
      attestations.proofs.clear();
      attestations.proofs.emplace_back(proof);
    }
    return aggregated_attestations;
  }

  void ForkChoiceStore::prune(Slot finalized_slot) {
    auto should_retain = [&](const AttestationData &data) {
      return data.target.slot > finalized_slot;
    };
    retain_if(attestations_by_data_,
              [&](const decltype(attestations_by_data_)::value_type &p) {
                return should_retain(p.second.data);
              });
    updateMetricGossipSignatures();
  }

  void ForkChoiceStore::updateMetricGossipSignatures() {
    size_t metric_signatures = 0;
    for (auto &batch : attestations_by_data_ | std::views::values) {
      metric_signatures += batch.signatures.size();
    }
    metrics_->lean_gossip_signatures()->set(metric_signatures);
  }

  void ForkChoiceStore::updateMetricAttestationSignature(bool valid) const {
    (valid ? metrics_->lean_pq_sig_attestation_signatures_valid_total()
           : metrics_->lean_pq_sig_attestation_signatures_invalid_total())
        ->inc();
  }
}  // namespace lean
