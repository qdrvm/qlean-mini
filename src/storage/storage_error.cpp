/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/storage_error.hpp"

OUTCOME_CPP_DEFINE_CATEGORY(lean::storage, StorageError, e) {
  using E = StorageError;
  switch (e) {
    case E::OK:
      return "success";
    case E::NOT_SUPPORTED:
      return "operation is not supported in storage";
    case E::CORRUPTION:
      return "data corruption in storage";
    case E::INVALID_ARGUMENT:
      return "invalid argument to storage";
    case E::IO_ERROR:
      return "IO error in storage";
    case E::NOT_FOUND:
      return "entry not found in storage";
    case E::DB_PATH_NOT_CREATED:
      return "storage path was not created";
    case E::STORAGE_GONE:
      return "storage instance has been uninitialized";
    case E::UNKNOWN:
      break;
  }

  return "unknown error";
}
