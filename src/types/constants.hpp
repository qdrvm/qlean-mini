/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>

#include <qtils/byte_arr.hpp>
#include <qtils/literals.hpp>

using qtils::literals::operator""_bytes;

namespace lean {

  static constexpr uint64_t SLOT_DURATION_MS = 4000;  // 4 seconds
  static constexpr uint64_t SECONDS_PER_INTERVAL = 1;
  static constexpr uint64_t SECONDS_PER_SLOT = 4;
  static constexpr uint64_t INTERVALS_PER_SLOT = 4;  // 4 intervals by 1 second

  // State list lengths

  static constexpr uint64_t HISTORICAL_ROOTS_LIMIT =
      1 << 18;  // 262'144 roots,	12.1 days
  static constexpr uint64_t VALIDATOR_REGISTRY_LIMIT = 1 << 12;  // 4'096 val

  // Networking

  /// Maximum number of blocks in a single request
  static constexpr uint64_t MAX_REQUEST_BLOCKS = 1 << 10;  // 1024

  using DomainType = qtils::ByteArr<4>;

  /// 4-byte domain for gossip message-id isolation of *invalid* snappy messages
  static constexpr DomainType MESSAGE_DOMAIN_INVALID_SNAPPY = {0, 0, 0, 0};
  /// 4-byte domain for gossip message-id isolation of *valid* snappy messages
  static constexpr DomainType MESSAGE_DOMAIN_VALID_SNAPPY = {1, 0, 0, 0};

}  // namespace lean
