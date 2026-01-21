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
#include "types/state.hpp"

namespace lean {

  STF::STF(qtils::SharedRef<metrics::Metrics> metrics, log::Logger logger)
      : metrics_(std::move(metrics)), log_(std::move(logger)) {}

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
   * Saves a map of `root -> justifications` back into the state's flattened
   * data structure.
   *
   * Corresponds to flatten_justifications_map in Python spec.
   */
  inline void setJustifications(State &state,
                                const Justifications &justifications) {
    auto &roots = state.justifications_roots.data();
    auto &validators = state.justifications_validators.data();
    roots.clear();
    roots.reserve(justifications.size());
    validators.clear();
    validators.reserve(justifications.size() * state.validatorCount());
    for (auto &[root, bits] : justifications) {
      BOOST_ASSERT(bits.size() == state.validatorCount());
      roots.push_back(root);
      validators.insert(validators.end(), bits.begin(), bits.end());
    }
  }

  State STF::generateGenesisState(
      const Config &config,
      std::span<const crypto::xmss::XmssPublicKey> validators_pubkeys) {
    BlockHeader header;
    header.slot = 0;
    header.proposer_index = 0;
    header.parent_root = kZeroHash;
    header.state_root = kZeroHash;
    header.body_root = sszHash(BlockBody{});

    State result;
    result.config = config;
    result.slot = 0;
    result.latest_block_header = header;
    result.latest_justified = Checkpoint{.root = kZeroHash, .slot = 0};
    result.latest_finalized = Checkpoint{.root = kZeroHash, .slot = 0};

    ValidatorIndex validator_index = 0;
    for (auto &validator_pubkey : validators_pubkeys) {
      result.validators.push_back(Validator{
          .pubkey = validator_pubkey,
          .index = validator_index,
      });
      ++validator_index;
    }
    // result.historical_block_hashes;
    // result.justified_slots;
    // result.justifications_roots;
    // result.justifications_validators;

    return result;
  }

  Block STF::genesisBlock(const State &state) {
    Block result;
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

    // Step through each missing slot:
    while (state.slot < slot) {
      // Per-Slot Housekeeping & Slot Increment
      //
      // This performs two tasks for each empty slot:
      //
      // 1. State Root Caching (Conditional):
      //    Check if the latest block header has an empty state root.
      //    This is true only for the *first* empty slot immediately
      //    following a block.
      //
      //    - If it is empty, we must cache the pre-block state root
      //      (the hash of the state *before* this slot increment) into that
      //      header.
      //
      //    - If the state root is *not* empty, it means we are in a
      //      sequence of empty slots, and no action is needed.
      //
      // 2. Slot Increment:
      //    Always increment the slot number by one.
      //
      if (state.latest_block_header.state_root == kZeroHash) {
        state.latest_block_header.state_root = sszHash(state);
      }
      ++state.slot;
      metrics_->stf_slots_processed_total()->inc();
    }
    return outcome::success();
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
    // Validation
    auto &parent_header = state.latest_block_header;
    parent_header.updateHash();
    auto parent_root = parent_header.hash();

    // The block must be for the current slot.
    if (block.slot != state.slot) {
      return Error::INVALID_SLOT;
    }

    // The block must be newer than the current latest header.
    if (block.slot <= parent_header.slot) {
      return Error::INVALID_SLOT;
    }

    // The proposer must be the expected validator for this slot.
    if (not validateProposerIndex(state, block)) {
      return Error::INVALID_PROPOSER;
    }

    // The declared parent must match the hash of the latest block header.
    if (block.parent_root != parent_root) {
      return Error::PARENT_ROOT_DOESNT_MATCH;
    }

    // State Updates

    // Special case: first block after genesis.
    //
    // Mark genesis as both justified and finalized.
    bool is_genesis_parent = parent_header.slot == 0;
    [[unlikely]] if (is_genesis_parent) {
      // block.parent_root is the genesis root
      state.latest_justified.root = parent_root;
      state.latest_finalized.root = parent_root;
    }

    // If there were empty slots between parent and this block, fill them.
    auto num_empty_slots = block.slot - parent_header.slot - 1;

    // Build new historical hashes list
    //
    // Now that we can vote on parent, push it at its correct slot index in the
    // structures.
    state.historical_block_hashes.push_back(parent_root);

    // If there were empty slots, push zero hash for those ancestors
    for (auto i = num_empty_slots; i > 0; --i) {
      state.historical_block_hashes.push_back(kZeroHash);
    }

    // Build new justified slots list
    //
    // Genesis block is always justified
    state.justified_slots.push_back(is_genesis_parent);

    // Mark empty slots as not justified
    for (auto i = num_empty_slots; i > 0; --i) {
      state.justified_slots.push_back(false);
    }

    // Construct the new latest block header.
    //
    // Leave state_root empty; it will be filled on the next process_slot call.
    state.latest_block_header = block.getHeader();
    state.latest_block_header.state_root = kZeroHash;

    return outcome::success();
  }

  outcome::result<void> STF::processOperations(State &state,
                                               const BlockBody &body) const {
    // process attestations
    OUTCOME_TRY(processAttestations(state, body.attestations));
    // other operations will get added as the functionality evolves
    return outcome::success();
  }

  outcome::result<void> STF::processAttestations(
      State &state, const AggregatedAttestations &attestations) const {
    auto timer = metrics_->stf_attestations_processing_time_seconds()->timer();

    // NOTE:
    // The state already contains three pieces of data:
    //   1. A list of block roots that have received justification votes.
    //   2. A long sequence of boolean entries representing all validator votes,
    //      flattened into a single list.
    //   3. The total number of validators.
    //
    // The flattened vote list is organized so that votes from all validators
    // for each block root appear together, and those groups are simply placed
    // back-to-back.
    //
    // To work with attestations, we must rebuild the intuitive structure:
    //   "for each block root, here is the list of validator votes for it".
    //
    // Reconstructing this is done by cutting the long vote list into
    // consecutive segments, where:
    //   - each segment corresponds to one block root,
    //   - each segment has length equal to the number of validators,
    //   - and the ordering of block roots is preserved.
    Justifications justifications;
    if (state.justifications_roots.size() > 0) {
      auto &roots = state.justifications_roots.data();
      auto &flat_justifications = state.justifications_validators.data();
      size_t offset = 0;
      BOOST_ASSERT(flat_justifications.size()
                   == roots.size() * state.validatorCount());
      for (auto &root : roots) {
        auto next_offset = offset + state.validatorCount();
        std::vector<bool> bits{
            flat_justifications.begin() + offset,
            flat_justifications.begin() + next_offset,
        };
        justifications[root] = std::move(bits);
        offset = next_offset;
      }
    }

    // Track state changes to be applied at the end
    auto latest_justified = state.latest_justified;
    auto latest_finalized = state.latest_finalized;
    auto justified_slots = state.justified_slots.data();

    // Process each attestation in the block.
    for (auto &attestation : attestations) {
      auto &attestation_data = attestation.data;
      auto &source = attestation_data.source;
      auto &target = attestation_data.target;

      // Ignore attestations whose source is not already justified,
      // or whose target is not in the history, or whose target is not a
      // valid justifiable slot
      auto source_slot = source.slot;
      auto target_slot = target.slot;

      if (source_slot >= state.historical_block_hashes.size()) {
        return Error::INVALID_VOTE_SOURCE_SLOT;
      }
      if (target_slot >= state.historical_block_hashes.size()) {
        return Error::INVALID_VOTE_TARGET_SLOT;
      }

      // Source slot must be justified
      if (not getBit(justified_slots, source_slot)) {
        continue;
      }

      // Target slot must not be already justified
      // This condition is missing in 3sf mini but has been added here because
      // we don't want to re-introduce the target again for remaining votes if
      // the slot is already justified and its tracking already cleared out
      // from justifications map
      if (getBit(justified_slots, target_slot)) {
        continue;
      }

      // Source root must match the state's historical block hashes
      if (source.root != state.historical_block_hashes.data().at(source_slot)) {
        continue;
      }

      // Target root must match the state's historical block hashes
      if (target.root != state.historical_block_hashes.data().at(target_slot)) {
        continue;
      }

      // Target slot must be after source slot
      if (target.slot <= source.slot) {
        continue;
      }

      // Target slot must be justifiable after the latest finalized slot
      if (not isJustifiableSlot(latest_finalized.slot, target.slot)) {
        continue;
      }

      // Track attempts to justify new hashes
      auto justifications_it = justifications.find(target.root);
      if (justifications_it == justifications.end()) {
        justifications_it =
            justifications.emplace(target.root, std::vector<bool>{}).first;
        justifications_it->second.resize(state.validatorCount());
      }

      for (auto &&validator_id :
           getAggregatedValidators(attestation.aggregation_bits)) {
        if (validator_id >= justifications_it->second.size()) {
          return Error::INVALID_VOTER;
        }
        if (not justifications_it->second.at(validator_id)) {
          justifications_it->second.at(validator_id) = true;
        }
      }

      size_t count = std::ranges::count(justifications_it->second, true);

      // If 2/3 attested to the same new valid hash to justify
      // in 3sf mini this is strict equality, but we have updated it to >=
      // also have modified it from count >= (2 * state.config.num_validators)
      // // 3 to prevent integer division which could lead to less than 2/3 of
      // validators justifying specially if the num_validators is low in
      // testing scenarios
      if (3 * count >= 2 * state.validatorCount()) {
        latest_justified = target;
        metrics_->stf_latest_justified_slot()->set(latest_justified.slot);
        setBit(justified_slots, target_slot);
        justifications.erase(target.root);

        // Finalization: if the target is the next valid justifiable
        // hash after the source
        auto any = false;
        for (auto slot = source_slot + 1; slot < target_slot; ++slot) {
          if (isJustifiableSlot(latest_finalized.slot, slot)) {
            any = true;
            break;
          }
        }
        if (not any) {
          latest_finalized = source;
          metrics_->stf_latest_finalized_slot()->set(latest_finalized.slot);
        }
      }
      metrics_->stf_attestations_processed_total()->inc();
    }

    // Flatten and set updated justifications back to the state
    setJustifications(state, justifications);

    // Apply tracked state changes
    state.justified_slots.data() = std::move(justified_slots);
    state.latest_justified = latest_justified;
    state.latest_finalized = latest_finalized;

    return outcome::success();
  }

  bool STF::validateProposerIndex(const State &state,
                                  const Block &block) const {
    return block.proposer_index == block.slot % state.validatorCount();
  }
}  // namespace lean
