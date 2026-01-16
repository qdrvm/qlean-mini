/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/block_storage_error.hpp"

OUTCOME_CPP_DEFINE_CATEGORY(lean::blockchain, BlockStorageError, e) {
  using E = BlockStorageError;
  switch (e) {
    case E::BLOCK_EXISTS:
      return "Block already exists on the chain";
    case E::NO_BLOCKS_FOUND:
      return "No blocks found";
    case E::HEADER_NOT_FOUND:
      return "Block header was not found";
    case BlockStorageError::ATTESTATION_NOT_FOUND:
      return "Block attestation was not found";
    case BlockStorageError::SIGNATURE_NOT_FOUND:
      return "Block signature was not found";
    case BlockStorageError::BODY_NOT_FOUND:
      return "Block body was not found";
    case BlockStorageError::INCONSISTENT_DATA:
      return "Inconsistent data";
    case E::GENESIS_BLOCK_ALREADY_EXISTS:
      return "Genesis block already exists";
    case E::FINALIZED_BLOCK_NOT_FOUND:
      return "Finalized block not found. Possibly storage is corrupted";
    case E::GENESIS_BLOCK_NOT_FOUND:
      return "Genesis block not found";
    case E::BLOCK_TREE_LEAVES_NOT_FOUND:
      return "Block tree leaves not found";
    case E::JUSTIFICATION_EMPTY:
      return "Justification empty";
  }
  return "Unknown error";
}
