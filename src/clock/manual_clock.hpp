/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include "clock/clock.hpp"

namespace lean::clock {

  /**
   * Mock implementation of SystemClock for testing purposes.
   * Allows manual control of time progression.
   */
  class ManualClock : public SystemClock {
   public:
    ManualClock() = default;

    explicit ManualClock(std::chrono::milliseconds initial_time_msec)
        : current_time_msec_(initial_time_msec) {}

    TimePoint now() const override {
      return TimePoint(std::chrono::milliseconds(current_time_msec_));
    }

    uint64_t nowSec() const override {
      return std::chrono::duration_cast<std::chrono::seconds>(
                 current_time_msec_)
          .count();
    }

    std::chrono::milliseconds nowMsec() const override {
      return std::chrono::milliseconds(current_time_msec_);
    }

    /**
     * Advance the mock time by the specified number of milliseconds
     */
    void advance(std::chrono::milliseconds milliseconds) {
      current_time_msec_ += milliseconds;
    }

    /**
     * Set the mock time to a specific value in milliseconds
     */
    void setTime(std::chrono::milliseconds milliseconds) {
      current_time_msec_ = milliseconds;
    }

   private:
    std::chrono::milliseconds current_time_msec_;
  };

}  // namespace lean::clock
