/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <optional>

#include <types/constants.hpp>

namespace lean {
  using Slot = uint64_t;
  using TimestampSeconds = uint64_t;

  /**
   * Time measured in intervals.
   */
  struct Interval {
    static Interval fromSlot(Slot slot, uint64_t phase) {
      return Interval{.interval = slot * INTERVALS_PER_SLOT + phase};
    }

    static std::optional<Interval> fromTime(std::chrono::milliseconds time,
                                            const auto &config) {
      if (time < config.genesisTimeMs()) {
        return std::nullopt;
      }
      return Interval{
          .interval = static_cast<uint64_t>((time - config.genesisTimeMs())
                                            / INTERVAL_DURATION_MS),
      };
    }

    Slot slot() const {
      return interval / INTERVALS_PER_SLOT;
    }

    uint64_t phase() const {
      return interval % INTERVALS_PER_SLOT;
    }

    std::chrono::milliseconds time(const auto &config) const {
      return config.genesisTimeMs() + interval * INTERVAL_DURATION_MS;
    }

    uint64_t interval;
  };
}  // namespace lean
