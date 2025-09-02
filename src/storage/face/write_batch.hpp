/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Interface for batch write operations on storage.
 *
 * A WriteBatch accumulates multiple write operations and applies them
 * in a single atomic commit to improve performance and ensure consistency.
 */

#pragma once

#include "storage/face/writeable.hpp"

namespace lean::storage::face {

  /**
   * @brief An abstraction over a storage, which can be used for batch writes.
   *
   * @tparam K Key type.
   * @tparam V Value type.
   *
   * A WriteBatch implementation collects multiple write operations and
   * applies them together when commit() is invoked. After committing,
   * the batch can be cleared and reused.
   */
  template <typename K, typename V>
  struct WriteBatch : public Writeable<K, V> {
    /**
     * @brief Writes batch.
     *
     * @details Executes all accumulated write operations in this batch
     * and applies them atomically to the underlying storage.
     *
     * @return outcome::result<void> Result indicating success or containing
     * an error code in case of failure.
     */
    virtual outcome::result<void> commit() = 0;

    /**
     * @brief Clear batch.
     *
     * @details Removes all pending write operations from this batch, resetting
     * it to an empty state and allowing reuse without creating a new instance.
     */
    virtual void clear() = 0;
  };

}  // namespace lean::storage::face
