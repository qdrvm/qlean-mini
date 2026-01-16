/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/impl/block_storage_impl.hpp"

#include <qtils/cxx23/ranges/contains.hpp>

#include "blockchain/block_storage_error.hpp"
#include "blockchain/impl/storage_util.hpp"
#include "sszpp/ssz++.hpp"
#include "storage/predefined_keys.hpp"
#include "types/block_data.hpp"
#include "types/state.hpp"

namespace lean::blockchain {

  BlockStorageImpl::BlockStorageImpl(
      qtils::SharedRef<log::LoggingSystem> logsys,
      qtils::SharedRef<storage::SpacedStorage> storage,
      qtils::SharedRef<crypto::Hasher> hasher,
      std::shared_ptr<BlockStorageInitializer>)
      : logger_(logsys->getLogger("BlockStorage", "block_storage")),
        storage_(std::move(storage)),
        hasher_(std::move(hasher)) {}

  outcome::result<std::vector<BlockHash>> BlockStorageImpl::getBlockTreeLeaves()
      const {
    if (block_tree_leaves_.has_value()) {
      return block_tree_leaves_.value();
    }

    auto default_space = storage_->getSpace(storage::Space::Default);
    OUTCOME_TRY(leaves_opt,
                default_space->tryGet(storage::kBlockTreeLeavesLookupKey));
    if (not leaves_opt.has_value()) {
      return BlockStorageError::BLOCK_TREE_LEAVES_NOT_FOUND;
    }
    auto &encoded_leaves = leaves_opt.value();

    OUTCOME_TRY(leaves, decode<std::vector<BlockHash>>(encoded_leaves));

    block_tree_leaves_.emplace(std::move(leaves));

    return block_tree_leaves_.value();
  }

  outcome::result<void> BlockStorageImpl::setBlockTreeLeaves(
      std::vector<BlockHash> leaves) {
    std::ranges::sort(leaves);

    if (block_tree_leaves_.has_value()
        and block_tree_leaves_.value() == leaves) {
      return outcome::success();
    }

    auto default_space = storage_->getSpace(storage::Space::Default);
    OUTCOME_TRY(encoded_leaves, encode(leaves));
    OUTCOME_TRY(default_space->put(storage::kBlockTreeLeavesLookupKey,
                                   qtils::ByteVec{std::move(encoded_leaves)}));

    block_tree_leaves_.emplace(std::move(leaves));

    return outcome::success();
  }

  outcome::result<BlockIndex> BlockStorageImpl::getLastFinalized() const {
    OUTCOME_TRY(leaves, getBlockTreeLeaves());
    auto current_hash = leaves[0];
    for (;;) {
      // OUTCOME_TRY(j_opt, getJustification(current_hash));
      // if (j_opt.has_value()) {
      //   break;
      // } // FIXME
      OUTCOME_TRY(header, getBlockHeader(current_hash));
      if (header.slot == 0) {
        SL_TRACE(logger_,
                 "Not found block with justification. "
                 "Genesis block will be used as last finalized ({})",
                 current_hash);
        return {0, current_hash};  // genesis
      }
      current_hash = header.parent_root;
    }

    OUTCOME_TRY(header, getBlockHeader(current_hash));
    auto found_block = BlockIndex{header.slot, current_hash};
    SL_TRACE(logger_,
             "Justification is found in block {}. "
             "This block will be used as last finalized",
             found_block);
    return found_block;
  }

  outcome::result<void> BlockStorageImpl::assignHashToSlot(
      const BlockIndex &block_index) {
    SL_DEBUG(logger_, "Add slot-to-hash for {}", block_index);
    auto slot_to_hash_key = slotToHashLookupKey(block_index.slot);
    auto storage = storage_->getSpace(storage::Space::SlotToHashes);
    OUTCOME_TRY(hashes, getBlockHash(block_index.slot));
    if (not qtils::cxx23::ranges::contains(hashes, block_index.hash)) {
      hashes.emplace_back(block_index.hash);
      OUTCOME_TRY(storage->put(slot_to_hash_key, block_index.hash));
    }
    return outcome::success();
  }

  outcome::result<void> BlockStorageImpl::deassignHashToSlot(
      const BlockIndex &block_index) {
    SL_DEBUG(logger_, "Remove num-to-idx for {}", block_index);
    auto slot_to_hash_key = slotToHashLookupKey(block_index.slot);
    auto storage = storage_->getSpace(storage::Space::SlotToHashes);
    OUTCOME_TRY(hashes, getBlockHash(block_index.slot));
    auto to_erase = std::ranges::remove(hashes, block_index.hash);
    if (not to_erase.empty()) {
      hashes.erase(to_erase.begin(), to_erase.end());
      if (hashes.empty()) {
        OUTCOME_TRY(storage->remove(slot_to_hash_key));
      } else {
        OUTCOME_TRY(storage->put(slot_to_hash_key, encode(hashes).value()));
      }
    }
    return outcome::success();
  }

  outcome::result<std::vector<BlockHash>> BlockStorageImpl::getBlockHash(
      Slot slot) const {
    auto storage = storage_->getSpace(storage::Space::SlotToHashes);
    OUTCOME_TRY(data_opt, storage->tryGet(slotToHashLookupKey(slot)));
    if (data_opt.has_value()) {
      return decode<std::vector<BlockHash>>(data_opt.value());
    }
    return {{}};
  }

  outcome::result<BlockStorage::SlotIterator> BlockStorageImpl::seekLastSlot()
      const {
    auto storage = storage_->getSpace(storage::Space::SlotToHashes);
    return SlotIterator::create(storage->cursor());
  }

  outcome::result<bool> BlockStorageImpl::hasBlockHeader(
      const BlockHash &block_hash) const {
    return hasInSpace(*storage_, storage::Space::Header, block_hash);
  }

  outcome::result<BlockHash> BlockStorageImpl::putBlockHeader(
      const BlockHeader &header) {
    OUTCOME_TRY(encoded_header, encode(header));
    header.updateHash();
    const auto &block_hash = header.hash();
    OUTCOME_TRY(putToSpace(*storage_,
                           storage::Space::Header,
                           block_hash,
                           std::move(encoded_header)));
    return block_hash;
  }

  outcome::result<BlockHeader> BlockStorageImpl::getBlockHeader(
      const BlockHash &block_hash) const {
    OUTCOME_TRY(header_opt, fetchBlockHeader(block_hash));
    if (header_opt.has_value()) {
      return header_opt.value();
    }
    return BlockStorageError::HEADER_NOT_FOUND;
  }

  outcome::result<std::optional<BlockHeader>>
  BlockStorageImpl::tryGetBlockHeader(const BlockHash &block_hash) const {
    return fetchBlockHeader(block_hash);
  }

  outcome::result<void> BlockStorageImpl::putBlockBody(
      const BlockHash &block_hash, const BlockBody &block_body) {
    OUTCOME_TRY(encoded_body, encode(block_body));
    return putToSpace(
        *storage_, storage::Space::Body, block_hash, std::move(encoded_body));
  }

  outcome::result<std::optional<BlockBody>> BlockStorageImpl::getBlockBody(
      const BlockHash &block_hash) const {
    OUTCOME_TRY(encoded_block_body_opt,
                getFromSpace(*storage_, storage::Space::Body, block_hash));
    if (encoded_block_body_opt.has_value()) {
      OUTCOME_TRY(block_body,
                  decode<BlockBody>(encoded_block_body_opt.value()));
      return std::make_optional(std::move(block_body));
    }
    return std::nullopt;
  }

  outcome::result<void> BlockStorageImpl::removeBlockBody(
      const BlockHash &block_hash) {
    auto space = storage_->getSpace(storage::Space::Body);
    return space->remove(block_hash);
  }

  outcome::result<void> BlockStorageImpl::putState(const BlockHash &block_hash,
                                                   const State &state) {
    OUTCOME_TRY(encoded_state, encode(state));
    return putToSpace(
        *storage_, storage::Space::State, block_hash, std::move(encoded_state));
  }

  outcome::result<std::optional<State>> BlockStorageImpl::getState(
      const BlockHash &block_hash) const {
    OUTCOME_TRY(encoded_state_opt,
                getFromSpace(*storage_, storage::Space::State, block_hash));
    if (encoded_state_opt.has_value()) {
      OUTCOME_TRY(state, decode<State>(encoded_state_opt.value()));
      return std::make_optional(std::move(state));
    }
    return std::nullopt;
  }

  outcome::result<void> BlockStorageImpl::removeState(
      const BlockHash &block_hash) {
    auto space = storage_->getSpace(storage::Space::State);
    return space->remove(block_hash);
  }

  outcome::result<BlockHash> BlockStorageImpl::putBlock(
      const BlockData &block) {
    auto adding_res = [&]() -> outcome::result<BlockHash> {
      // insert provided block's parts into the database
      OUTCOME_TRY(block_hash, putBlockHeader(*block.header));

      if (block.attestation.has_value()) {
        OUTCOME_TRY(encoded_attestation, encode(*block.attestation));
        OUTCOME_TRY(putToSpace(*storage_,
                               storage::Space::Attestation,
                               block_hash,
                               std::move(encoded_attestation)));
      }

      if (block.signature.has_value()) {
        OUTCOME_TRY(encoded_attestation, encode(*block.signature));
        OUTCOME_TRY(putToSpace(*storage_,
                               storage::Space::Signature,
                               block_hash,
                               std::move(encoded_attestation)));
      }

      if (block.body.has_value()) {
        OUTCOME_TRY(encoded_body, encode(*block.body));
        OUTCOME_TRY(putToSpace(*storage_,
                               storage::Space::Body,
                               block_hash,
                               std::move(encoded_body)));
      }

      return block_hash;
    }();

    if (adding_res.has_value()) {
      auto block_hash = adding_res.value();

      logger_->info("Added block {} as child of {}",
                    BlockIndex{block.header->slot, block_hash},
                    block.header->parent_root);
      return block_hash;
    }

    std::ignore = removeBlock(block.header->hash());
    return adding_res.error();
  }

  outcome::result<BlockData> BlockStorageImpl::getBlock(
      const BlockHash &block_hash, BlockParts parts) const {
    BlockData data{.hash = block_hash};

    // Block header
    if (parts & BlockParts::HEADER) {
      OUTCOME_TRY(header, getBlockHeader(block_hash));
      data.header.emplace(header);
    }

    // Block signature
    if (parts & BlockParts::SIGNATURES) {
      OUTCOME_TRY(
          encoded_signature_opt,
          getFromSpace(*storage_, storage::Space::Signature, block_hash));
      if (encoded_signature_opt.has_value()) {
        OUTCOME_TRY(signature,
                    decode<BlockSignatures>(encoded_signature_opt.value()));
        data.signature.emplace(std::move(signature));
      } else {
        return BlockStorageError::SIGNATURE_NOT_FOUND;
      }
    }

    // Block attestation
    if (parts & BlockParts::ATTESTATION) {
      OUTCOME_TRY(
          encoded_attestation_opt,
          getFromSpace(*storage_, storage::Space::Attestation, block_hash));
      if (encoded_attestation_opt.has_value()) {
        OUTCOME_TRY(attestation,
                    decode<Attestations>(encoded_attestation_opt.value()));
        data.attestation.emplace(std::move(attestation));
      } else {
        return BlockStorageError::ATTESTATION_NOT_FOUND;
      }
    }

    // Block body
    if (parts & BlockParts::BODY) {
      OUTCOME_TRY(encoded_body_opt,
                  getFromSpace(*storage_, storage::Space::Body, block_hash));
      if (encoded_body_opt.has_value()) {
        OUTCOME_TRY(body, decode<BlockBody>(encoded_body_opt.value()));
        data.body.emplace(std::move(body));
      } else {
        return BlockStorageError::BODY_NOT_FOUND;
      }
    }

    return data;
  }

  outcome::result<SignedBlockWithAttestation>
  BlockStorageImpl::getSignedBlockWithAttestation(
      const BlockHash &block_hash) const {
    OUTCOME_TRY(data, getBlock(block_hash, BlockParts::ALL));

    SignedBlockWithAttestation block;

    // Block header
    block.message.block.parent_root = data.header->parent_root;
    block.message.block.slot = data.header->slot;
    block.message.block.proposer_index = data.header->proposer_index;
    block.message.block.state_root = data.header->state_root;

    // Block signature
    block.signature = std::move(*data.signature);

    // Block attestation
    auto &attestations = *data.attestation;
    auto it = std::ranges::find_if(attestations, [&](const auto &attestation) {
      return attestation.validator_id == block.message.block.proposer_index;
    });
    BOOST_ASSERT(it != attestations.end());
    if (it == attestations.end()) [[unlikely]] {
      return BlockStorageError::INCONSISTENT_DATA;
    }
    block.message.proposer_attestation = *it;

    // Block body
    block.message.block.body = std::move(*data.body);

    return block;
  }

  outcome::result<void> BlockStorageImpl::removeBlock(
      const BlockHash &block_hash) {
    // Check if block still in storage
    OUTCOME_TRY(header_opt, fetchBlockHeader(block_hash));
    if (not header_opt) {
      return outcome::success();
    }
    const auto &header = header_opt.value();

    auto block_index = header.index();

    SL_TRACE(logger_, "Removing block {}…", block_index);

    {  // Remove slot-to-hash assigning
      auto num_to_hash_key = slotToHashLookupKey(block_index.slot);

      auto key_space = storage_->getSpace(storage::Space::SlotToHashes);
      OUTCOME_TRY(hash_opt, key_space->tryGet(num_to_hash_key.view()));
      if (hash_opt == block_hash) {
        if (auto res = key_space->remove(num_to_hash_key); res.has_error()) {
          SL_ERROR(logger_,
                   "could not remove slot-to-hash of {} from the storage: {}",
                   block_index,
                   res.error());
          return res;
        }
        SL_DEBUG(logger_, "Removed slot-to-hash of {}", block_index);
      }
    }

    // TODO(xDimon): needed to clean up trie storage if block deleted

    // Remove the block body
    if (auto res = removeBlockBody(block_index.hash); res.has_error()) {
      SL_ERROR(logger_,
               "could not remove body of block {} from the storage: {}",
               block_index,
               res.error());
      return res;
    }

    {  // Remove the block header
      auto header_space = storage_->getSpace(storage::Space::Header);
      if (auto res = header_space->remove(block_index.hash); res.has_error()) {
        SL_ERROR(logger_,
                 "could not remove header of block {} from the storage: {}",
                 block_index,
                 res.error());
        return res;
      }
    }

    logger_->info("Removed block {}", block_index);

    return outcome::success();
  }

  outcome::result<std::optional<BlockHeader>>
  BlockStorageImpl::fetchBlockHeader(const BlockHash &block_hash) const {
    OUTCOME_TRY(encoded_header_opt,
                getFromSpace(*storage_, storage::Space::Header, block_hash));
    if (encoded_header_opt.has_value()) {
      auto &encoded_header = encoded_header_opt.value();
      OUTCOME_TRY(header, decode<BlockHeader>(encoded_header));
      BOOST_ASSERT((header.updateHash(), header.hash() == block_hash));
      return std::make_optional(header);
    }
    return std::nullopt;
  }

}  // namespace lean::blockchain
