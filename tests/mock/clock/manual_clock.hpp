/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "clock/clock.hpp"

#include <memory>

namespace lean::clock {

  /**
   * Mock implementation of SystemClock for testing purposes.
   * Allows manual control of time progression.
   */
  class ManualClock : public SystemClock {
   public:
    ManualClock() : current_time_msec_(0) {}
    
    explicit ManualClock(uint64_t initial_time_msec)
        : current_time_msec_(initial_time_msec) {}

    TimePoint now() const override {
      return TimePoint(std::chrono::milliseconds(current_time_msec_));
    }

    uint64_t nowSec() const override {
      return current_time_msec_ / 1000;
    }

    uint64_t nowMsec() const override {
      return current_time_msec_;
    }

    /**
     * Advance the mock time by the specified number of milliseconds
     */
    void advance(uint64_t milliseconds) {
      current_time_msec_ += milliseconds;
    }

    /**
     * Set the mock time to a specific value in milliseconds
     */
    void setTime(uint64_t milliseconds) {
      current_time_msec_ = milliseconds;
    }

   private:
    uint64_t current_time_msec_;
  };

}  // namespace lean::clock