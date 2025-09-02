/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Interface mixin for batched write support.
 *
 * BatchWriteable exposes a method to create a WriteBatch
 * for efficient bulk updates to a map-like storage.
 */

#pragma once

#include <memory>

#include "storage/face/write_batch.hpp"

namespace lean::storage::face {

  /**
   * @brief Mixin interface for batched map modifications.
   * @tparam K Key type.
   * @tparam V Value type.
   *
   * BatchWriteable implementations provide a method to create
   * write batches that group multiple write operations for
   * atomic and efficient application.
   */
  template <typename K, typename V>
  struct BatchWriteable {
    virtual ~BatchWriteable() = default;

    /**
     * @brief Create a new write batch.
     *
     * @return std::unique_ptr<WriteBatch<K, V>> A batch object
     * for efficient bulk writes. The default implementation throws
     * logic_error if not overridden.
     */
    virtual std::unique_ptr<WriteBatch<K, V>> batch() {
      throw std::logic_error{"BatchWriteable::batch not implemented"};
    }
  };

}  // namespace lean::storage::face
