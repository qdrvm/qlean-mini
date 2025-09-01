/**
* Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file in_memory_spaced_storage.hpp
 * @brief Implements an in-memory version of SpacedStorage for testing purposes.
 *
 * This class provides an in-memory implementation of the SpacedStorage interface.
 * It is useful for unit tests and other scenarios where a persistent backend is
 * not required or desired.
 */

#pragma once

#include <memory>

#include "in_memory_storage.hpp"
#include "storage/buffer_map_types.hpp"
#include "storage/spaced_storage.hpp"

namespace lean::storage {

  /**
   * @class InMemorySpacedStorage
   * @brief In-memory implementation of the SpacedStorage interface.
   *
   * This class maps Space identifiers to corresponding instances of
   * InMemoryStorage. It is typically used in tests to simulate isolated
   * persistent storage spaces without involving a real database.
   */
  class InMemorySpacedStorage : public storage::SpacedStorage {
  public:
    /**
     * @brief Retrieve or create an in-memory storage for a given space.
     *
     * If the storage for the given space already exists, returns it.
     * Otherwise, creates a new InMemoryStorage and stores it.
     *
     * @param space The logical storage space to retrieve.
     * @return A shared pointer to the corresponding BufferStorage.
     */
    std::shared_ptr<BufferStorage> getSpace(Space space) override {
      auto it = spaces_.find(space);
      if (it != spaces_.end()) {
        return it->second;
      }
      return spaces_.emplace(space, std::make_shared<InMemoryStorage>())
          .first->second;
    }

  private:
    /// Map of storage spaces to their corresponding in-memory storages
    std::map<Space, std::shared_ptr<InMemoryStorage>> spaces_;
  };

}  // namespace lean::storage