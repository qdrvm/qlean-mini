/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "clock/clock.hpp"

namespace lean::clock {

  template <typename ClockType>
  class ClockImpl : virtual public Clock<ClockType> {
   public:
    typename Clock<ClockType>::TimePoint now() const override;
    uint64_t nowSec() const override;
    uint64_t nowMsec() const override;
  };

  class SystemClockImpl : public SystemClock,
                          public ClockImpl<std::chrono::system_clock> {};
  class SteadyClockImpl : public SteadyClock,
                          public ClockImpl<std::chrono::steady_clock> {};

}  // namespace lean::clock
