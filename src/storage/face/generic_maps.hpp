/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Composite interface for generic key-value storage.
 *
 * Combines readable, writable, iterable, and batched write support
 * into a single storage abstraction.
 */

#pragma once

#include "storage/face/batch_writeable.hpp"
#include "storage/face/iterable.hpp"
#include "storage/face/readable.hpp"
#include "storage/face/writeable.hpp"

namespace lean::storage::face {

  /**
   * @brief Abstraction over a key-value storage supporting read, write,
   * iteration, and batch writes.
   * @tparam K Key type.
   * @tparam V Value type.
   *
   * GenericStorage merges multiple storage interfaces to provide a unified
   * API for key-value operations.
   */
  template <typename K, typename V>
  struct GenericStorage : Readable<K, V>,
                          Iterable<K, V>,
                          Writeable<K, V>,
                          BatchWriteable<K, V> {
    /**
     * @brief Hint for approximate RAM usage.
     *
     * @return std::optional<size_t> Optional in-memory size in bytes,
     * or std::nullopt if no size hint is available.
     */
    [[nodiscard]] virtual std::optional<size_t> byteSizeHint() const {
      return std::nullopt;
    }
  };

}  // namespace lean::storage::face
