/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/state_transition_function.hpp"

#include <boost/assert.hpp>
#include <soralog/macro.hpp>

#include "blockchain/is_justifiable_slot.hpp"
#include "metrics/metrics.hpp"
#include "types/signed_block.hpp"
#include "types/state.hpp"

namespace lean {
  constexpr BlockHash kZeroHash;

  STF::STF(qtils::SharedRef<metrics::Metrics> metrics)
      : metrics_(std::move(metrics)) {}

  inline bool getBit(const std::vector<bool> &bits, size_t i) {
    return i < bits.size() and bits.at(i);
  }

  inline void setBit(std::vector<bool> &bits, size_t i) {
    if (bits.size() <= i) {
      bits.resize(i + 1);
    }
    bits.at(i) = true;
  }

  using Justifications = std::map<BlockHash, std::vector<bool>>;

  /**
   * Returns a map of `root -> justifications` constructed from the flattened
   * data in the state.
   */
  inline Justifications getJustifications(const State &state) {
    auto &roots = state.justifications_roots.data();
    auto &validators = state.justifications_validators.data();
    Justifications justifications;
    size_t offset = 0;
    BOOST_ASSERT(validators.size()
                 == roots.size() * state.config.num_validators);
    for (auto &root : roots) {
      auto next_offset = offset + state.config.num_validators;
      std::vector<bool> bits{
          validators.begin() + offset,
          validators.begin() + next_offset,
      };
      justifications[root] = std::move(bits);
      offset = next_offset;
    }
    return justifications;
  }

  /**
   * Saves a map of `root -> justifications` back into the state's flattened
   * data structure.
   */
  inline void setJustifications(State &state,
                                const Justifications &justifications) {
    auto &roots = state.justifications_roots.data();
    auto &validators = state.justifications_validators.data();
    roots.clear();
    roots.reserve(justifications.size());
    validators.clear();
    validators.reserve(justifications.size() * state.config.num_validators);
    for (auto &[root, bits] : justifications) {
      BOOST_ASSERT(bits.size() == state.config.num_validators);
      roots.push_back(root);
      validators.insert(validators.end(), bits.begin(), bits.end());
    }
  }

  AnchorState STF::generateGenesisState(const Config &config) {
    BlockHeader header;
    header.slot = 0;
    header.proposer_index = 0;
    header.parent_root = kZeroHash;
    header.state_root = kZeroHash;
    header.body_root = sszHash(BlockBody{});

    AnchorState result;
    result.config = config;
    result.slot = 0;
    result.latest_block_header = header;
    result.latest_justified = Checkpoint{.root = kZeroHash, .slot = 0};
    result.latest_finalized = Checkpoint{.root = kZeroHash, .slot = 0};
    // result.historical_block_hashes;
    // result.justified_slots;
    // result.justifications_roots;
    // result.justifications_validators;

    return result;
  }

  AnchorBlock STF::genesisBlock(const State &state) {
    AnchorBlock result;
    result.slot = state.slot;
    result.proposer_index = 0;
    result.parent_root = kZeroHash;
    result.state_root = sszHash(state);
    result.body = BlockBody{};
    return result;
  }

  outcome::result<State> STF::stateTransition(const Block &block,
                                              const State &parent_state,
                                              bool check_state_root) const {
    auto timer = metrics_->stf_state_transition_time_seconds()->timer();
    auto state = parent_state;
    // Process slots (including those with no blocks) since block
    OUTCOME_TRY(processSlots(state, block.slot));
    // Process block
    OUTCOME_TRY(processBlock(state, block));
    // Verify state root
    if (check_state_root) {
      auto state_root = sszHash(state);
      if (block.state_root != state_root) {
        return Error::STATE_ROOT_DOESNT_MATCH;
      }
    }
    return std::move(state);
  }

  outcome::result<void> STF::processSlots(State &state, Slot slot) const {
    auto timer = metrics_->stf_slots_processing_time_seconds()->timer();
    if (state.slot >= slot) {
      return Error::INVALID_SLOT;
    }
    while (state.slot < slot) {
      processSlot(state);
      ++state.slot;
      metrics_->stf_slots_processed_total()->inc();
    }
    return outcome::success();
  }

  void STF::processSlot(State &state) const {
    // Cache latest block header state root
    if (state.latest_block_header.state_root == kZeroHash) {
      state.latest_block_header.state_root = sszHash(state);
    }
  }

  outcome::result<void> STF::processBlock(State &state,
                                          const Block &block) const {
    auto timer = metrics_->stf_block_processing_time_seconds()->timer();
    OUTCOME_TRY(processBlockHeader(state, block));
    OUTCOME_TRY(processOperations(state, block.body));
    return outcome::success();
  }

  outcome::result<void> STF::processBlockHeader(State &state,
                                                const Block &block) const {
    // Verify that the slots match
    if (block.slot != state.slot) {
      return Error::INVALID_SLOT;
    }
    // Verify that the block is newer than latest block header
    if (block.slot <= state.latest_block_header.slot) {
      return Error::INVALID_SLOT;
    }
    // Verify that proposer index is the correct index
    if (not validateProposerIndex(state, block)) {
      return Error::INVALID_PROPOSER;
    }
    // Verify that the parent matches
    state.latest_block_header.updateHash();
    if (block.parent_root != state.latest_block_header.hash()) {
      return Error::PARENT_ROOT_DOESNT_MATCH;
    }

    // If this was first block post genesis, 3sf mini special treatment is
    // required to correctly set genesis block root as already justified and
    // finalized. This is not possible at the time of genesis state generation
    // and are set at zero bytes because genesis block is calculated using
    // genesis state causing a circular dependency
    [[unlikely]] if (state.latest_block_header.slot == 0) {
      // block.parent_root is the genesis root
      state.latest_justified.root = block.parent_root;
      state.latest_finalized.root = block.parent_root;
    }

    // now that we can vote on parent, push it at its correct slot index in the
    // structures
    state.historical_block_hashes.push_back(block.parent_root);
    // genesis block is always justified
    state.justified_slots.push_back(state.latest_block_header.slot == 0);

    // if there were empty slots, push zero hash for those ancestors
    for (auto num_empty_slots = block.slot - state.latest_block_header.slot - 1;
         num_empty_slots > 0;
         --num_empty_slots) {
      state.historical_block_hashes.push_back(kZeroHash);
      state.justified_slots.push_back(false);
    }

    // Cache current block as the new latest block
    state.latest_block_header = block.getHeader();
    // Overwritten in the next process_slot call
    state.latest_block_header.state_root = kZeroHash;
    return outcome::success();
  }

  outcome::result<void> STF::processOperations(State &state,
                                               const BlockBody &body) const {
    // process attestations
    OUTCOME_TRY(processAttestations(state, body.attestations.data()));
    // other operations will get added as the functionality evolves
    return outcome::success();
  }

  outcome::result<void> STF::processAttestations(
      State &state, const std::vector<SignedVote> &attestations) const {
    // get justifications, justified slots and historical block hashes are
    // already upto date as per the processing in process_block_header
    auto timer = metrics_->stf_attestations_processing_time_seconds()->timer();
    auto justifications = getJustifications(state);

    // From 3sf-mini/consensus.py - apply votes
    for (auto &signed_vote : attestations) {
      auto &vote = signed_vote.data;
      if (vote.source.slot >= state.historical_block_hashes.size()) {
        return Error::INVALID_VOTE_SOURCE_SLOT;
      }
      if (vote.target.slot >= state.historical_block_hashes.size()) {
        return Error::INVALID_VOTE_TARGET_SLOT;
      }
      // Ignore votes whose source is not already justified,
      // or whose target is not in the history, or whose target is not a
      // valid justifiable slot
      if (not getBit(state.justified_slots.data(), vote.source.slot)
          // This condition is missing in 3sf mini but has been added here
          // because we don't want to re-introduce the target again for
          // remaining votes if the slot is already justified and its tracking
          // already cleared out from justifications map
          or getBit(state.justified_slots.data(), vote.target.slot)
          or vote.source.root
                 != state.historical_block_hashes.data().at(vote.source.slot)
          or vote.target.root
                 != state.historical_block_hashes.data().at(vote.target.slot)
          or vote.target.slot <= vote.source.slot
          or not isJustifiableSlot(state.latest_finalized.slot,
                                   vote.target.slot)) {
        continue;
      }

      auto justifications_it = justifications.find(vote.target.root);
      // Track attempts to justify new hashes
      if (justifications_it == justifications.end()) {
        justifications_it =
            justifications.emplace(vote.target.root, std::vector<bool>{}).first;
        justifications_it->second.resize(state.config.num_validators);
      }

      if (signed_vote.validator_id >= justifications_it->second.size()) {
        return Error::INVALID_VOTER;
      }
      justifications_it->second.at(signed_vote.validator_id) = true;

      size_t count = std::ranges::count(justifications_it->second, true);

      // If 2/3 voted for the same new valid hash to justify
      // in 3sf mini this is strict equality, but we have updated it to >=
      // also have modified it from count >= (2 * state.config.num_validators)
      // / 3 to prevent integer division which could lead to less than 2/3 of
      // validators justifying specially if the num_validators is low in testing
      // scenarios
      if (3 * count >= 2 * state.config.num_validators) {
        state.latest_justified = vote.target;
        metrics_->stf_latest_justified_slot()->set(state.latest_justified.slot);
        setBit(state.justified_slots.data(), vote.target.slot);
        justifications.erase(vote.target.root);

        // Finalization: if the target is the next valid justifiable hash after
        // the source
        auto any = false;
        for (auto slot = vote.source.slot + 1; slot < vote.target.slot;
             ++slot) {
          if (isJustifiableSlot(state.latest_finalized.slot, slot)) {
            any = true;
            break;
          }
        }
        if (not any) {
          state.latest_finalized = vote.source;
          metrics_->stf_latest_finalized_slot()->set(
              state.latest_finalized.slot);
        }
      }
      metrics_->stf_attestations_processed_total()->inc();
    }

    // flatten and set updated justifications back to the state
    setJustifications(state, justifications);
    return outcome::success();
  }

  bool STF::validateProposerIndex(const State &state,
                                  const Block &block) const {
    return block.proposer_index == block.slot % state.config.num_validators;
  }
}  // namespace lean
