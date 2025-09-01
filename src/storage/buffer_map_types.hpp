/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Convenience typedefs for ByteVec-based storage interfaces.
 *
 * Defines specializations and using aliases for storage interfaces
 * when keys and values are qtils::ByteVec.
 */

#pragma once

#include <qtils/byte_vec_or_view.hpp>

#include "storage/face/generic_maps.hpp"
#include "storage/face/write_batch.hpp"

namespace lean::storage::face {

  /**
   * @brief OwnedOrView trait for ByteVec values.
   *
   * Resolves to ByteVecOrView, allowing either an owned container
   * or a view over ByteVec data.
   */
  template <>
  struct OwnedOrViewTrait<qtils::ByteVec> {
    using type = qtils::ByteVecOrView;
  };

  /**
   * @brief ViewTrait for ByteVec keys.
   *
   * Resolves to ByteView, providing a view over ByteVec key data.
   */
  template <>
  struct ViewTrait<qtils::ByteVec> {
    using type = qtils::ByteView;
  };

}  // namespace lean::storage::face

namespace lean::storage {

  using qtils::ByteVec;
  using qtils::ByteVecOrView;
  using qtils::ByteView;

  /**
   * @brief Alias for a byte-vector write batch.
   *
   * Provides a WriteBatch specialized for ByteVec keys and values.
   */
  using BufferBatch = face::WriteBatch<ByteVec, ByteVec>;

  /**
   * @brief Alias for generic byte-vector storage.
   *
   * Combines read, write, iteration, and batch support for ByteVec.
   */
  using BufferStorage = face::GenericStorage<ByteVec, ByteVec>;

  /**
   * @brief Cursor type for iterating over byte-vector storage.
   */
  using BufferStorageCursor = face::MapCursor<ByteVec, ByteVec>;

}  // namespace lean::storage
