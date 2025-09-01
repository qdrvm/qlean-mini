/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Interface for batch-modifiable storage map.
 *
 * Writeable provides methods to atomically add or remove entries
 * in the underlying storage by key.
 */

#pragma once

#include <qtils/outcome.hpp>

#include "storage/face/owned_or_view.hpp"
#include "storage/face/view.hpp"

namespace lean::storage::face {

  /**
   * @brief Interface for batch-modifiable map storage.
   * @tparam K Key type.
   * @tparam V Value type.
   *
   * Writeable is a mixin that exposes methods to put or remove
   * entries in a map-like storage. Implementations should apply
   * each operation immediately or as part of a larger batch.
   */
  template <typename K, typename V>
  struct Writeable {
    virtual ~Writeable() = default;

    /**
     * @brief Store or update a value by key.
     *
     * @param key   Key to associate with the value.
     * @param value The value to store, either owned or a view.
     * @return outcome::result<void> Returns void on success or
     * an error code on failure.
     */
    virtual outcome::result<void> put(const View<K> &key,
                                      OwnedOrView<V> &&value) = 0;

    /**
     * @brief Remove a value by key.
     *
     * @param key Key whose mapping should be removed.
     * @return outcome::result<void> Returns void on success or
     * an error code if the removal fails.
     */
    virtual outcome::result<void> remove(const View<K> &key) = 0;
  };

}  // namespace lean::storage::face
