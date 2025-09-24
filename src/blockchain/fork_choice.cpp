/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/fork_choice.hpp"

#include <iostream>
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

  void ForkChoiceStore::tickInterval(bool has_proposal) {
    ++time_;
    auto current_interval = time_ % INTERVALS_PER_SLOT;
    if (current_interval == 0) {
      if (has_proposal) {
        acceptNewVotes();
      }
    } else if (current_interval == 1) {
      // validators will vote in this interval using safe target previously
      // computed
    } else if (current_interval == 2) {
      updateSafeTarget();
    } else {
      acceptNewVotes();
    }
  }

  void ForkChoiceStore::advanceTime(Interval time, bool has_proposal) {
    // Calculate the number of intervals that have passed since genesis
    auto tick_interval_time =
        (time - config_.genesis_time) / SECONDS_PER_INTERVAL;

    // Tick the store one interval at a time until the target time is reached
    while (time_ < tick_interval_time) {
      // Determine if a proposal should be signaled for the next interval
      auto should_signal_proposal =
          has_proposal and (time_ + 1) == tick_interval_time;

      // Tick the interval and potentially signal a proposal
      tickInterval(should_signal_proposal);
    }
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
    auto current_slot = time_ / SECONDS_PER_INTERVAL;
    if (vote.slot > current_slot + 1) {
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
      auto time_slots = time_ / SECONDS_PER_INTERVAL;
      if (vote.slot > time_slots) {
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

  ForkChoiceStore getForkchoiceStore(State anchor_state, Block anchor_block) {
    BOOST_ASSERT(anchor_block.state_root == sszHash(anchor_state));
    anchor_block.setHash();
    auto anchor_root = anchor_block.hash();
    ForkChoiceStore store{
        .time_ = anchor_block.slot * INTERVALS_PER_SLOT,
        .config_ = anchor_state.config,
        .head_ = anchor_root,
        .safe_target_ = anchor_root,

        // TODO: ensure latest justified and finalized are set correctly
        .latest_justified_ = Checkpoint::from(anchor_block),
        .latest_finalized_ = Checkpoint::from(anchor_block),
    };
    store.blocks_.emplace(anchor_root, std::move(anchor_block));
    store.states_.emplace(anchor_root, std::move(anchor_state));
    std::cout << "Genesis (anchor) root " << anchor_root.toHex() << "\n";
    return store;
  }
}  // namespace lean
