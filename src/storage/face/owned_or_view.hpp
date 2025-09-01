/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Trait to select between owned or view type for storage values.
 *
 * OwnedOrViewTrait defines a member type `type` that resolves to either
 * an owning container or a view type for T. The alias OwnedOrView<T>
 * simplifies accessing the resolved type.
 */

#pragma once

namespace lean::storage::face {

  /**
   * @brief Trait to determine the storage value type.
   *
   * Specialize this trait to define `type` as either an owned
   * container or a view for the template parameter T.
   *
   * @tparam T Underlying value type.
   */
  template <typename T>
  struct OwnedOrViewTrait;

  /**
   * @brief Alias to the resolved owned or view type.
   *
   * @tparam T Underlying value type.
   * @see OwnedOrViewTrait
   */
  template <typename T>
  using OwnedOrView = typename OwnedOrViewTrait<T>::type;

}  // namespace lean::storage::face
