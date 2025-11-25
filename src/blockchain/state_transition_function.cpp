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
    // No justified roots means no justifications to reconstruct.
    [[unlikely]] if (state.justifications_roots.size() == 0) { return {}; }

    auto &roots = state.justifications_roots.data();
    auto &validators = state.justifications_validators.data();
    Justifications justifications;
    size_t offset = 0;
    BOOST_ASSERT(validators.size() == roots.size() * state.validatorCount());
    for (auto &root : roots) {
      auto next_offset = offset + state.validatorCount();
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
    validators.reserve(justifications.size() * state.validatorCount());
    for (auto &[root, bits] : justifications) {
      BOOST_ASSERT(bits.size() == state.validatorCount());
      roots.push_back(root);
      validators.insert(validators.end(), bits.begin(), bits.end());
    }
  }

  AnchorState STF::generateGenesisState(
      const Config &config,
      qtils::SharedRef<ValidatorRegistry> registry,
      qtils::SharedRef<app::ValidatorKeysManifest> validator_keys_manifest) {
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

    for (size_t i = 0; i < registry->allValidatorsIndices().size(); ++i) {
      auto opt_pubkey = validator_keys_manifest->getXmssPubkeyByIndex(i);
      if (not opt_pubkey) {
        continue;
      }
      result.validators.push_back(Validator{.pubkey = *opt_pubkey});
    }
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
      State &state, const Attestations &attestations) const {
    // get justifications, justified slots and historical block hashes are
    // already upto date as per the processing in process_block_header
    auto timer = metrics_->stf_attestations_processing_time_seconds()->timer();
    auto justifications = getJustifications(state);

    // From 3sf-mini/consensus.py - apply attestations
    for (auto &attestation : attestations) {
      auto &attestation_data = attestation.data;
      if (attestation_data.source.slot
          >= state.historical_block_hashes.size()) {
        return Error::INVALID_VOTE_SOURCE_SLOT;
      }
      if (attestation_data.target.slot
          >= state.historical_block_hashes.size()) {
        return Error::INVALID_VOTE_TARGET_SLOT;
      }
      // Ignore attestations whose source is not already justified,
      // or whose target is not in the history, or whose target is not a
      // valid justifiable slot
      if (not getBit(state.justified_slots.data(), attestation_data.source.slot)
          // This condition is missing in 3sf mini but has been added here
          // because we don't want to re-introduce the target again for
          // remaining attestations if the slot is already justified and its
          // tracking already cleared out from justifications map
          or getBit(state.justified_slots.data(), attestation_data.target.slot)
          or attestation_data.source.root
                 != state.historical_block_hashes.data().at(
                     attestation_data.source.slot)
          or attestation_data.target.root
                 != state.historical_block_hashes.data().at(
                     attestation_data.target.slot)
          or attestation_data.target.slot <= attestation_data.source.slot
          or not isJustifiableSlot(state.latest_finalized.slot,
                                   attestation_data.target.slot)) {
        continue;
      }

      auto justifications_it =
          justifications.find(attestation_data.target.root);
      // Track attempts to justify new hashes
      if (justifications_it == justifications.end()) {
        justifications_it =
            justifications
                .emplace(attestation_data.target.root, std::vector<bool>{})
                .first;
        justifications_it->second.resize(state.validatorCount());
      }

      if (attestation.validator_id >= justifications_it->second.size()) {
        return Error::INVALID_VOTER;
      }
      justifications_it->second.at(attestation.validator_id) = true;

      size_t count = std::ranges::count(justifications_it->second, true);

      // If 2/3 voted for the same new valid hash to justify
      // in 3sf mini this is strict equality, but we have updated it to >=
      // also have modified it from count >= (2 * state.config.num_validators)
      // / 3 to prevent integer division which could lead to less than 2/3 of
      // validators justifying specially if the num_validators is low in testing
      // scenarios
      if (3 * count >= 2 * state.validatorCount()) {
        state.latest_justified = attestation_data.target;
        metrics_->stf_latest_justified_slot()->set(state.latest_justified.slot);
        setBit(state.justified_slots.data(), attestation_data.target.slot);
        justifications.erase(attestation_data.target.root);

        // Finalization: if the target is the next valid justifiable hash after
        // the source
        auto any = false;
        for (auto slot = attestation_data.source.slot + 1;
             slot < attestation_data.target.slot;
             ++slot) {
          if (isJustifiableSlot(state.latest_finalized.slot, slot)) {
            any = true;
            break;
          }
        }
        if (not any) {
          state.latest_finalized = attestation_data.source;
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
    return block.proposer_index == block.slot % state.validatorCount();
  }
}  // namespace lean
