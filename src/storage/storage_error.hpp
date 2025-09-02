/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Definitions of storage interface error codes.
 *
 * Provides an enumeration of error codes that can be returned by
 * storage operations, along with integration into outcome error handling.
 */

#pragma once

#include <qtils/enum_error_code.hpp>

namespace lean::storage {

  /**
   * @brief Universal error codes for storage interface.
   *
   * Defines common error conditions returned by storage operations,
   * such as missing entries, corruption, or IO failures.
   */
  enum class StorageError : int {  // NOLINT(performance-enum-size)

    OK = 0,  ///< success (no error)

    NOT_SUPPORTED = 1,        ///< operation is not supported in storage
    CORRUPTION = 2,           ///< data corruption in storage
    INVALID_ARGUMENT = 3,     ///< invalid argument to storage
    IO_ERROR = 4,             ///< IO error in storage
    NOT_FOUND = 5,            ///< entry not found in storage
    DB_PATH_NOT_CREATED = 6,  ///< storage path was not created
    STORAGE_GONE = 7,         ///< storage instance has been uninitialized

    UNKNOWN = 1000,  ///< unknown error
  };
}  // namespace lean::storage

/**
 * @brief Declare StorageError integration with Outcome library.
 *
 * Enables automatic conversion between StorageError and
 * outcome::result for error handling.
 */
OUTCOME_HPP_DECLARE_ERROR(lean::storage, StorageError);
