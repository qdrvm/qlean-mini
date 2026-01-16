/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/enum_error_code.hpp>

namespace testutil {
  /**
   * @name Dummy error
   * @brief Special error for using instead special error and avoid need linkage
   * additional external library, i.e. in mock-object, call expectation, etc.
   * This provides several error codes for cases with different error.
   */
  enum class DummyError { ERROR = 1, ERROR_2, ERROR_3, ERROR_4, ERROR_5 };
}  // namespace testutil

OUTCOME_HPP_DECLARE_ERROR(testutil, DummyError);

inline OUTCOME_CPP_DEFINE_CATEGORY(testutil, DummyError, e) {
  using testutil::DummyError;
  switch (e) {
    case DummyError::ERROR:
      return "dummy error";
    case DummyError::ERROR_2:
      return "dummy error #2";
    case DummyError::ERROR_3:
      return "dummy error #3";
    case DummyError::ERROR_4:
      return "dummy error #4";
    case DummyError::ERROR_5:
      return "dummy error #5";
  }
  return "unknown (DummyError) error";
}
