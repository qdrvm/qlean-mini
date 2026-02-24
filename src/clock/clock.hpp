/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <chrono>

namespace lean::clock {

  /**
   * An interface for a clock
   * @tparam clock type is an underlying clock type, such as std::steady_clock
   */
  template <typename ClockType>
  class Clock {
   public:
    /**
     * Difference between two time points
     */
    using Duration = typename ClockType::duration;

    /**
     * A moment in time, stored in milliseconds since Unix epoch start
     */
    using TimePoint = typename ClockType::time_point;

    virtual ~Clock() = default;

    /**
     * @return a time point representing the current time
     */
    [[nodiscard]] virtual TimePoint now() const = 0;

    /**
     * @return uint64_t representing number of seconds since the beginning of
     * epoch (Jan 1, 1970)
     */
    [[nodiscard]] virtual uint64_t nowSec() const = 0;

    /**
     * @return uint64_t representing number of milliseconds since the beginning
     * of epoch (Jan 1, 1970)
     */
    [[nodiscard]] virtual std::chrono::milliseconds nowMsec() const = 0;

    static TimePoint zero() {
      return TimePoint{};
    }
  };

  /**
   * SystemClock alias over Clock. Should be used when we need to watch current
   * time
   */
  class SystemClock : public virtual Clock<std::chrono::system_clock> {};

  /**
   * SteadyClock alias over Clock. Should be used when we need to measure
   * interval between two moments in time
   */
  class SteadyClock : public virtual Clock<std::chrono::steady_clock> {};

}  // namespace lean::clock
