/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

namespace qtils {
  /**
   * Make `shared_ptr` from object of type `T`.
   */
  template <typename T>
  auto toSharedPtr(T &&t) {
    return std::make_shared<std::remove_cvref_t<T>>(std::forward<T>(t));
  }
}  // namespace qtils
