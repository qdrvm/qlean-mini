/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/fork_choice.hpp"

#include <ranges>

#include "types/signed_block.hpp"

namespace lean {
  void ForkChoiceStore::updateSafeTarget() {
    // 2/3rd majority min voting voting weight for target selection
    auto min_target_score = ceilDiv(config_.num_validators * 2, 3);

    safe_target_ = getForkChoiceHead(
        blocks_, latest_justified_, latest_new_votes_, min_target_score);
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
    head_ =
        getForkChoiceHead(blocks_, latest_justified_, latest_known_votes_, 0);

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

  void ForkChoiceStore::acceptNewVotes() {
    for (auto &[voter, vote] : latest_new_votes_) {
      latest_known_votes_[voter] = vote;
    }
    latest_new_votes_.clear();
    updateHead();
  }

  Slot ForkChoiceStore::getCurrentSlot() {
    // Calculate the current slot based on the elapsed time since genesis.
    // Each slot is SLOT_DURATION_MS milliseconds long.
    uint64_t now = clock_->nowMsec();
    Slot current_slot = (now - config_.genesis_time) / SLOT_DURATION_MS;
    return current_slot;
  }


  BlockHash ForkChoiceStore::getHead() {
    return head_;
  }
  State ForkChoiceStore::getState(const BlockHash &block_hash) const {
    auto it = states_.find(block_hash);
    if (it == states_.end()) {
      throw std::out_of_range("No state for block hash");
    }
    return it->second;
  }

  bool ForkChoiceStore::hasBlock(const BlockHash &hash) const {
    return blocks_.contains(hash);
  }

  Slot ForkChoiceStore::getBlockSlot(const BlockHash &block_hash) const {
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

  Checkpoint ForkChoiceStore::getVoteTarget() const {
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

  outcome::result<Block> ForkChoiceStore::produceBlock(Slot slot,
                                                     ValidatorIndex validator_index) {
    if (validator_index != slot % config_.num_validators) {
      return Error::INVALID_PROPOSER;
    }
    const auto& head_root = getHead();
    const auto& head_state = getState(head_root);

    Attestations attestations;

    while (true) {
      // Create candidate block with current attestation set
      Block candidate_block{
          .slot = slot,
          .proposer_index = validator_index,
          .parent_root = head_root,
          .state_root = {},  // to be filled after state transition
          .body = BlockBody{.attestations = attestations}};

      // Apply state transition to get the post-block state
      // First advance state to target slot, then process the block
      auto state = head_state;  // get head state
      OUTCOME_TRY(stf_.processSlots(state, slot));  // advances state
      OUTCOME_TRY(stf_.processBlock(state, candidate_block));

      // Find new valid attestations matching post-state justification
      Attestations new_attestations;
      for (const auto &[validator_id, checkpoint] : latest_known_votes_) {
        // Skip if target block is unknown in our store
        if (not blocks_.contains(checkpoint.root)) {
          continue;
        }
        // Create attestation with post-state's latest justified as source
        SignedVote signed_vote{
            .data =
                Vote{
                    .validator_id = validator_id,
                    .slot = checkpoint.slot,
                    .head = checkpoint,
                    .target = checkpoint,
                    .source = state.latest_justified,
                },
            .signature = qtils::ByteArr<32>{0}  // signature with zero bytes for
                                                // now
        };
        // Only include if attestation is valid and not already included
        if (auto it = std::find(
                attestations.begin(), attestations.end(), signed_vote);
            it == attestations.end()) {
          new_attestations.push_back(signed_vote);
        }
      }
      // If no new attestations, we are done
      if (new_attestations.size() == 0) {
        break;
      }

      // Add new attestations and continue iteration
      for (auto &attestation : new_attestations) {
        attestations.push_back(attestation);
      }
    }
    // Create final block with all collected attestations
    auto final_state = head_state;
    OUTCOME_TRY(stf_.processSlots(final_state, slot));  // create final state
    Block final_block{.slot = slot,
                      .proposer_index = validator_index,
                      .parent_root = head_root,
                      .state_root = {},  // to be filled after state transition
                      .body = BlockBody{.attestations = attestations}};

    // Apply state transition to get final post-state and compute state root
    OUTCOME_TRY(stf_.processBlock(final_state, final_block));
    final_block.state_root = sszHash(final_state);

    // Store block and state in forkchoice store
    auto block_hash = sszHash(final_block);
    blocks_.emplace(block_hash, final_block);
    states_.emplace(block_hash, std::move(final_state));

    // update head (not in spec)
    head_ = block_hash;

    return final_block;
  }


  outcome::result<void> ForkChoiceStore::validateAttestation(
      const SignedVote &signed_vote) {
    auto &vote = signed_vote.data;

    // Validate vote targets exist in store
    if (not blocks_.contains(vote.source.root)) {
      return Error::INVALID_ATTESTATION;
    }
    if (not blocks_.contains(vote.target.root)) {
      return Error::INVALID_ATTESTATION;
    }

    // Validate slot relationships
    auto &source_block = blocks_.at(vote.source.root);
    auto &target_block = blocks_.at(vote.target.root);

    if (source_block.slot > target_block.slot) {
      return Error::INVALID_ATTESTATION;
    }
    if (vote.source.slot > vote.target.slot) {
      return Error::INVALID_ATTESTATION;
    }

    // Validate checkpoint slots match block slots
    if (source_block.slot != vote.source.slot) {
      return Error::INVALID_ATTESTATION;
    }
    if (target_block.slot != vote.target.slot) {
      return Error::INVALID_ATTESTATION;
    }

    // Validate attestation is not too far in the future
    if (vote.slot > getCurrentSlot() + 1) {
      return Error::INVALID_ATTESTATION;
    }

    return outcome::success();
  }

  outcome::result<void> ForkChoiceStore::processAttestation(
      const SignedVote &signed_vote, bool is_from_block) {
    signed_votes_[signed_vote.data.validator_id] = signed_vote;
    // Validate attestation structure and constraints
    BOOST_OUTCOME_TRY(validateAttestation(signed_vote));

    auto &validator_id = signed_vote.data.validator_id;
    auto &vote = signed_vote.data;

    if (is_from_block) {
      // update latest known votes if this is latest
      auto latest_known_vote = latest_known_votes_.find(validator_id);
      if (latest_known_vote == latest_known_votes_.end()
          or latest_known_vote->second.slot < vote.slot) {
        latest_known_votes_.insert_or_assign(validator_id, vote.target);
      }

      // clear from new votes if this is latest
      auto latest_new_vote = latest_new_votes_.find(validator_id);
      if (latest_new_vote != latest_new_votes_.end()
          and latest_new_vote->second.slot <= vote.target.slot) {
        latest_new_votes_.erase(latest_new_vote);
      }
    } else {
      // forkchoice should be correctly ticked to current time before importing
      // gossiped attestations
      if (vote.slot > getCurrentSlot() + 1) {
        return Error::INVALID_ATTESTATION;
      }

      // update latest new votes if this is the latest
      auto latest_new_vote = latest_new_votes_.find(validator_id);
      if (latest_new_vote == latest_new_votes_.end()
          or latest_new_vote->second.slot < vote.target.slot) {
        latest_new_votes_.insert_or_assign(validator_id, vote.target);
      }
    }

    return outcome::success();
  }

  outcome::result<void> ForkChoiceStore::onBlock(Block block) {
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
    auto state =
        stf_.stateTransition({.message = block}, parent_state, true).value();
    blocks_.emplace(block_hash, block);
    states_.emplace(block_hash, std::move(state));

    // add block votes to the onchain known last votes
    for (auto &signed_vote : block.body.attestations) {
      // Add block votes to the onchain known last votes
      BOOST_OUTCOME_TRY(processAttestation(signed_vote, true));
    }

    updateHead();

    return outcome::success();
  }

  BlockHash getForkChoiceHead(const ForkChoiceStore::Blocks &blocks,
                              const Checkpoint &root,
                              const ForkChoiceStore::Votes &latest_votes,
                              uint64_t min_score) {
    // If no votes, return the starting root immediately
    if (latest_votes.empty()) {
      return root.root;
    }

    // For each block, count the number of votes for that block. A vote for
    // any descendant of a block also counts as a vote for that block
    std::unordered_map<BlockHash, uint64_t> vote_weights;
    auto get_weight = [&](const BlockHash &hash) {
      auto it = vote_weights.find(hash);
      return it != vote_weights.end() ? it->second : 0;
    };

    for (auto &vote : latest_votes | std::views::values) {
      auto block_it = blocks.find(vote.root);
      if (block_it != blocks.end()) {
        while (block_it->second.slot > root.slot) {
          ++vote_weights[block_it->first];
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

  ForkChoiceStore::ForkChoiceStore(State anchor_state, Block anchor_block,
                                   std::shared_ptr<clock::SystemClock> clock) 
      : clock_(clock) {
    BOOST_ASSERT(anchor_block.state_root == sszHash(anchor_state));
    anchor_block.setHash();
    auto anchor_root = anchor_block.hash();
    config_ = anchor_state.config;
    head_ = anchor_root;
    safe_target_ = anchor_root;

    // TODO: ensure latest justified and finalized are set correctly
    latest_justified_ = Checkpoint::from(anchor_block);
    latest_finalized_ = Checkpoint::from(anchor_block);

    blocks_.emplace(anchor_root, std::move(anchor_block));
    states_.emplace(anchor_root, std::move(anchor_state));
  }

  // Test constructor implementation
  ForkChoiceStore::ForkChoiceStore(std::shared_ptr<clock::SystemClock> clock,
                                   Config config,
                                   BlockHash head,
                                   BlockHash safe_target,
                                   Checkpoint latest_justified,
                                   Checkpoint latest_finalized,
                                   Blocks blocks,
                                   std::unordered_map<BlockHash, State> states,
                                   Votes latest_known_votes,
                                   std::unordered_map<ValidatorIndex, SignedVote> signed_votes,
                                   Votes latest_new_votes)
      : clock_(clock),
        config_(config),
        head_(head),
        safe_target_(safe_target),
        latest_justified_(latest_justified),
        latest_finalized_(latest_finalized),
        blocks_(std::move(blocks)),
        states_(std::move(states)),
        latest_known_votes_(std::move(latest_known_votes)),
        signed_votes_(std::move(signed_votes)),
        latest_new_votes_(std::move(latest_new_votes)) {
  }
}  // namespace lean
