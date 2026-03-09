/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "clock/impl/clock_impl.hpp"

namespace lean::clock {

  template <typename ClockType>
  typename Clock<ClockType>::TimePoint ClockImpl<ClockType>::now() const {
    return ClockType::now();
  }

  template <typename ClockType>
  uint64_t ClockImpl<ClockType>::nowSec() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
               now().time_since_epoch())
        .count();
  }

  template <typename ClockType>
  std::chrono::milliseconds ClockImpl<ClockType>::nowMsec() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now().time_since_epoch());
  }

  template class ClockImpl<std::chrono::steady_clock>;
  template class ClockImpl<std::chrono::system_clock>;

}  // namespace lean::clock
