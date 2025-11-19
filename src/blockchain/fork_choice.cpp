/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/fork_choice.hpp"

#include <filesystem>
#include <ranges>
#include <stdexcept>

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

    safe_target_ = getForkChoiceHead(
        blocks_, latest_justified_, latest_new_attestations_, min_target_score);
  }

  std::optional<Checkpoint> ForkChoiceStore::getLatestJustified() {
    using Key = std::tuple<Slot, BlockHash>;
    std::optional<Key> max;
    for (auto &state : states_ | std::views::values) {
      Key key{state.latest_justified.slot, state.latest_justified.root};
      if (not max.has_value() or key > max.value()) {
        max = key;
      }
    }
    if (not max.has_value()) {
      return std::nullopt;
    }
    auto &[slot, hash] = max.value();
    if (slot == 0) {
      for (auto &[hash, block] : blocks_) {
        if (block.slot == 0) {
          return Checkpoint{.root = hash, .slot = slot};
        }
      }
    }
    return Checkpoint{.root = hash, .slot = slot};
  }

  void ForkChoiceStore::updateHead() {
    if (auto latest_justified = getLatestJustified()) {
      latest_justified_ = latest_justified.value();
      if (latest_justified_.slot == 0) {
        for (auto &[hash, block] : blocks_) {
          if (block.slot == 0) {
            latest_justified_.root = hash;
          }
        }
      }
    }
    head_ = getForkChoiceHead(
        blocks_, latest_justified_, latest_known_attestations_, 0);

    auto state_it = states_.find(head_);
    if (state_it != states_.end()) {
      latest_finalized_ = state_it->second.latest_finalized;
      if (latest_finalized_.slot == 0) {
        for (auto &[hash, block] : blocks_) {
          if (block.slot == 0) {
            latest_finalized_.root = hash;
          }
        }
      }
    }
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
    return blocks_.at(block_hash).slot;
  }

  Slot ForkChoiceStore::getHeadSlot() const {
    return blocks_.at(head_).slot;
  }

  const Config &ForkChoiceStore::getConfig() const {
    return config_;
  }

  Checkpoint ForkChoiceStore::getLatestFinalized() const {
    return latest_finalized_;
  }


  Checkpoint ForkChoiceStore::getAttestationTarget() const {
    // Start from head as target candidate
    auto target_block_root = head_;

    // If there is no very recent safe target, then vote for the k'th ancestor
    // of the head
    for (auto i = 0; i < 3; ++i) {
      if (blocks_.at(target_block_root).slot > blocks_.at(safe_target_).slot) {
        target_block_root = blocks_.at(target_block_root).parent_root;
      }
    }

    // If the latest finalized slot is very far back, then only some slots are
    // valid to justify, make sure the target is one of those
    while (not isJustifiableSlot(latest_finalized_.slot,
                                 blocks_.at(target_block_root).slot)) {
      target_block_root = blocks_.at(target_block_root).parent_root;
    }

    return Checkpoint{
        .root = target_block_root,
        .slot = blocks_.at(target_block_root).slot,
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

    // Store block and state in forkchoice store
    auto signed_block_with_attestation = signBlock({
        .message =
            {
                .block = block,
                .proposer_attestation = proposer_attestation,
            },
        .signature = signatures,
    });
    BOOST_OUTCOME_TRY(onBlock(signed_block_with_attestation));

    return signed_block_with_attestation;
  }

  Attestation ForkChoiceStore::produceAttestation(
      Slot slot, ValidatorIndex validator_index) {
    // Get the head block the validator sees for this slot
    Checkpoint head_checkpoint{
        .root = head_,
        .slot = getHeadSlot(),
    };

    //  Calculate the target checkpoint for this attestation
    //
    //  This uses the store's current forkchoice state to determine
    //  the appropriate attestation target, balancing between head
    //  advancement and safety guarantees.
    auto target_checkpoint = getAttestationTarget();

    // Construct attestation data
    AttestationData attestation_data{
        .slot = slot,
        .head = head_checkpoint,
        .target = target_checkpoint,
        .source = latest_justified_,
    };

    return Attestation{
        .validator_id = validator_index,
        .data = attestation_data,
    };
  }

  outcome::result<void> ForkChoiceStore::validateAttestation(
      const SignedAttestation &signed_attestation) {
    auto &attestation = signed_attestation.message;
    auto &data = attestation.data;

    SL_TRACE(logger_,
             "Validating attestation for target {}, source {}",
             data.target,
             data.source);
    auto timer = metrics_->fc_attestation_validation_time_seconds()->timer();

    // Validate vote targets exist in store
    if (not blocks_.contains(data.source.root)) {
      return Error::INVALID_ATTESTATION;
    }
    if (not blocks_.contains(data.target.root)) {
      return Error::INVALID_ATTESTATION;
    }

    // Validate slot relationships
    auto &source_block = blocks_.at(data.source.root);
    auto &target_block = blocks_.at(data.target.root);

    if (source_block.slot > target_block.slot) {
      return Error::INVALID_ATTESTATION;
    }
    if (data.source.slot > data.target.slot) {
      return Error::INVALID_ATTESTATION;
    }

    // Validate checkpoint slots match block slots
    if (source_block.slot != data.source.slot) {
      return Error::INVALID_ATTESTATION;
    }
    if (target_block.slot != data.target.slot) {
      return Error::INVALID_ATTESTATION;
    }

    // Validate attestation is not too far in the future
    if (data.slot > getCurrentSlot() + 1) {
      return Error::INVALID_ATTESTATION;
    }

    return outcome::success();
  }

  outcome::result<void> ForkChoiceStore::onAttestation(
      const SignedAttestation &signed_attestation, bool is_from_block) {
    // Validate attestation structure and constraints
    if (auto res = validateAttestation(signed_attestation); res.has_value()) {
      metrics_->fc_attestations_valid_total()->inc();
    } else {
      metrics_->fc_attestations_invalid_total()->inc();
      return res;
    }

    auto &attestation = signed_attestation.message;
    auto &validator_id = attestation.validator_id;
    auto &attestation_slot = attestation.data.slot;

    if (is_from_block) {
      // update latest known votes if this is latest
      auto latest_known_attestation =
          latest_known_attestations_.find(validator_id);
      if (latest_known_attestation == latest_known_attestations_.end()
          or latest_known_attestation->second.message.data.slot
                 < attestation_slot) {
        latest_known_attestations_.insert_or_assign(validator_id,
                                                    signed_attestation);
      }

      // clear from new votes if this is latest
      auto latest_new_attestation = latest_new_attestations_.find(validator_id);
      if (latest_new_attestation != latest_new_attestations_.end()
          and latest_new_attestation->second.message.data.slot
                  <= attestation_slot) {
        latest_new_attestations_.erase(latest_new_attestation);
      }
    } else {
      // forkchoice should be correctly ticked to current time before importing
      // gossiped attestations
      if (attestation_slot > getCurrentSlot() + 1) {
        return Error::INVALID_ATTESTATION;
      }

      // update latest new votes if this is the latest
      auto latest_new_attestation = latest_new_attestations_.find(validator_id);
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

  inline bool validateBlockSignatures(const Block &block,
                                      const BlockSignatures &signatures) {
    for (auto &signature : signatures) {
      if (not isValidSignature(signature)) {
        return false;
      }
    }
    return true;
  }

  outcome::result<void> ForkChoiceStore::processProposerAttestation(
      SignedBlockWithAttestation signed_block_with_attestation) {
    auto &block = signed_block_with_attestation.message.block;
    auto &proposer_attestation =
        signed_block_with_attestation.message.proposer_attestation;
    auto &signatures = signed_block_with_attestation.signature;

    if (block.body.attestations.size() >= signatures.size()) {
      return Error::INVALID_ATTESTATION;
    }
    // Proposer signature is at the end of signature list (after all block body
    // attestation signatures)
    auto &proposer_signature =
        signatures.data().at(block.body.attestations.size());
    SignedAttestation signed_proposer_attestation{
        .message = proposer_attestation,
        .signature = proposer_signature,
    };
    // Process as gossip (not from block) so it enters "new" attestations and
    // only influences fork choice after interval 3 acceptance
    BOOST_OUTCOME_TRY(onAttestation(signed_proposer_attestation, false));
    return outcome::success();
  }

  outcome::result<void> ForkChoiceStore::onBlock(
      SignedBlockWithAttestation signed_block_with_attestation) {
    auto timer = metrics_->fc_block_processing_time_seconds()->timer();
    auto &block = signed_block_with_attestation.message.block;
    auto &proposer_attestation =
        signed_block_with_attestation.message.proposer_attestation;
    auto &signatures = signed_block_with_attestation.signature;

    auto valid_signatures = validateBlockSignatures(block, signatures);

    block.setHash();
    auto block_hash = block.hash();
    // If the block is already known, ignore it
    if (blocks_.contains(block_hash)) {
      return outcome::success();
    }

    auto &parent_state = states_.at(block.parent_root);
    // at this point parent state should be available so node should sync parent
    // chain if not available before adding block to forkchoice

    // Get post state from STF (State Transition Function)
    BOOST_OUTCOME_TRY(auto state,
                      stf_.stateTransition(block, parent_state, true));
    blocks_.emplace(block_hash, block);
    states_.emplace(block_hash, std::move(state));

    // add block votes to the onchain known last votes
    ValidatorIndex index = 0;
    for (auto &attestation : block.body.attestations) {
      if (index >= signatures.size()) {
        return Error::INVALID_ATTESTATION;
      }
      auto &signature = signatures.data().at(index);
      SignedAttestation signed_attestation{
          .message = attestation,
          .signature = signature,
      };
      // Add block votes to the onchain known last votes
      BOOST_OUTCOME_TRY(onAttestation(signed_attestation, true));
      ++index;
    }

    updateHead();

    BOOST_OUTCOME_TRY(
        processProposerAttestation(signed_block_with_attestation));

    return outcome::success();
  }

  std::vector<std::variant<SignedAttestation, SignedBlockWithAttestation>>
  ForkChoiceStore::advanceTime(uint64_t now_sec) {
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
        auto source = getLatestJustified();
        SL_INFO(logger_,
                "For slot {}: head is {}, target is {}, source is {}",
                current_slot,
                head,
                target,
                source.value());
        for (auto validator_index :
             validator_registry_->currentValidatorIndices()) {
          if (isProposer(validator_index, current_slot, validator_count)) {
            continue;
          }
          auto attestation = produceAttestation(current_slot, validator_index);
          auto signed_attestation = signAttestation(attestation);

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

  BlockHash getForkChoiceHead(
      const ForkChoiceStore::Blocks &blocks,
      const Checkpoint &root,
      const ForkChoiceStore::AttestationMap &latest_attestations,
      uint64_t min_score) {
    // For each block, count the number of votes for that block. A vote for
    // any descendant of a block also counts as a vote for that block
    std::unordered_map<BlockHash, uint64_t> attestation_weights;
    auto get_weight = [&](const BlockHash &hash) {
      auto it = attestation_weights.find(hash);
      return it != attestation_weights.end() ? it->second : 0;
    };

    for (auto &attestation : latest_attestations | std::views::values) {
      auto block_it = blocks.find(attestation.message.data.target.root);
      if (block_it != blocks.end()) {
        while (block_it->second.slot > root.slot) {
          ++attestation_weights[block_it->first];
          block_it = blocks.find(block_it->second.parent_root);
          BOOST_ASSERT(block_it != blocks.end());
        }
      }
    }

    // Identify the children of each block
    using Key = std::tuple<uint64_t, Slot, BlockHash>;
    std::unordered_multimap<BlockHash, Checkpoint> children_map;
    for (auto &[hash, block] : blocks) {
      if (block.slot > root.slot and get_weight(hash) >= min_score) {
        children_map.emplace(block.parent_root, Checkpoint::from(block));
      }
    }

    // Start at the root (latest justified hash or genesis) and repeatedly
    // choose the child with the most latest votes, tiebreaking by slot then
    // hash
    auto current = root.root;
    while (true) {
      auto [begin, end] = children_map.equal_range(current);
      if (begin == end) {
        return current;
      }
      Key max;
      for (auto it = begin; it != end; ++it) {
        Key key{
            get_weight(it->second.root),
            it->second.slot,
            it->second.root,
        };
        if (it == begin or key > max) {
          max = key;
        }
      }
      current = std::get<2>(max);
    }
  }

  ForkChoiceStore::ForkChoiceStore(
      const GenesisConfig &genesis_config,
      qtils::SharedRef<clock::SystemClock> clock,
      qtils::SharedRef<log::LoggingSystem> logging_system,
      qtils::SharedRef<metrics::Metrics> metrics,
      qtils::SharedRef<ValidatorRegistry> validator_registry)
      : stf_(metrics),
        validator_registry_(validator_registry),
        logger_(
            logging_system->getLogger("ForkChoiceStore", "fork_choice_store")),
        metrics_(std::move(metrics)) {
    AnchorState anchor_state = STF::generateGenesisState(genesis_config.config);
    AnchorBlock anchor_block = STF::genesisBlock(anchor_state);
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

    blocks_.emplace(anchor_root, std::move(anchor_block));
    SL_INFO(
        logger_, "Anchor block {} at slot {}", anchor_root, anchor_block.slot);
    states_.emplace(anchor_root, std::move(anchor_state));
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
      AttestationMap latest_known_attestations,
      AttestationMap latest_new_attestations,
      ValidatorIndex validator_index,
      qtils::SharedRef<ValidatorRegistry> validator_registry)
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
        validator_registry_(std::move(validator_registry)) {}
}  // namespace lean
