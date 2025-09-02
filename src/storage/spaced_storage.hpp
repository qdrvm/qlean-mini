/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Declares the SpacedStorage interface, which provides access to
 *        separate logical storage spaces represented by BufferStorage.
 *
 * This interface is used as an abstraction for managing multiple namespaces
 * or isolated segments of storage within a broader system. Each space is
 * uniquely identified by a Space enum value.
 */

#pragma once

#include <memory>

#include "storage/buffer_map_types.hpp"
#include "storage/spaces.hpp"

namespace lean::storage {

  /**
   * @class SpacedStorage
   * @brief Abstract interface for accessing different logical storage spaces.
   *
   * The SpacedStorage class provides a mechanism to retrieve storage units
   * (BufferStorage) that correspond to different logical spaces within a
   * system. Implementations of this interface can be used to isolate and
   * organize data based on the space identifier.
   */
  class SpacedStorage {
   public:
    virtual ~SpacedStorage() = default;

    /**
     * Retrieve a pointer to the map representing particular storage space
     * @param space - identifier of required space
     * @return a pointer buffer storage for a space
     */
    virtual std::shared_ptr<BufferStorage> getSpace(Space space) = 0;
  };

}  // namespace lean::storage
