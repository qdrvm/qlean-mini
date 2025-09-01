/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Defines the Space enumeration used to identify logical storage spaces.
 *
 * The Space enum provides identifiers for different logical areas of storage
 * within the system. These values are used to access corresponding buffer
 * storages via the SpacedStorage interface.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace lean::storage {

  /**
   * @enum Space
   * @brief Enumerates the logical storage spaces used by the system.
   *
   * Each value in this enum represents a distinct namespace or segment
   * of the system's storage. The values are used as keys to retrieve
   * specific BufferStorage instances.
   */
  enum class Space : uint8_t {
    Default = 0,  ///< Default space used for general-purpose storage
    LookupKey,    ///< Space used for mapping lookup keys

    // application-defined spaces
    Header,
    Body,
    Justification,
    // ... append here

    Total  ///< Total number of defined spaces (must be last)
  };

  constexpr size_t SpacesCount = static_cast<size_t>(Space::Total);
}
