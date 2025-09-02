/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/byte_view.hpp>

#include "crypto/hash_types.hpp"

namespace lean::crypto {

  class Hasher {
   public:
    virtual ~Hasher() = default;

    /**
     * @brief twox_64 calculates 8-byte twox hash
     * @param data source data
     * @return 64-bit hash value
     */
    virtual Hash64 twox_64(qtils::ByteView data) const = 0;

    /**
     * @brief blake2b_64 function calculates 8-byte blake2b hash
     * @param data source value
     * @return 64-bit hash value
     */
    virtual Hash64 blake2b_64(qtils::ByteView data) const = 0;

    /**
     * @brief twox_128 calculates 16-byte twox hash
     * @param data source data
     * @return 128-bit hash value
     */
    virtual Hash128 twox_128(qtils::ByteView data) const = 0;

    /**
     * @brief blake2b_128 function calculates 16-byte blake2b hash
     * @param data source value
     * @return 128-bit hash value
     */
    virtual Hash128 blake2b_128(qtils::ByteView data) const = 0;

    /**
     * @brief twox_256 calculates 32-byte twox hash
     * @param data source data
     * @return 256-bit hash value
     */
    virtual Hash256 twox_256(qtils::ByteView data) const = 0;

    /**
     * @brief blake2b_256 function calculates 32-byte blake2b hash
     * @param data source value
     * @return 256-bit hash value
     */
    virtual Hash256 blake2b_256(qtils::ByteView data) const = 0;

    /**
     * @brief blake2b_512 function calculates 64-byte blake2b hash
     * @param data source value
     * @return 512-bit hash value
     */
    virtual Hash512 blake2b_512(qtils::ByteView data) const = 0;

    /**
     * @brief keccak_256 function calculates 32-byte keccak hash
     * @param data source value
     * @return 256-bit hash value
     */
    virtual Hash256 keccak_256(qtils::ByteView data) const = 0;

    /**
     * @brief blake2s_256 function calculates 32-byte blake2s hash
     * @param data source value
     * @return 256-bit hash value
     */
    virtual Hash256 blake2s_256(qtils::ByteView data) const = 0;

    /**
     * @brief sha2_256 function calculates 32-byte sha2-256 hash
     * @param data source value
     * @return 256-bit hash value
     */
    virtual Hash256 sha2_256(qtils::ByteView data) const = 0;
  };
}  // namespace lean::crypto
