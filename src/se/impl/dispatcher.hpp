/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <optional>

#include "scheduler.hpp"

namespace lean::se {

  /**
   * Interface for task dispatchers that handle execution across different threads
   */
  struct Dispatcher {
    using Tid = uint32_t;
    using Task = IScheduler::Task;
    using Predicate = IScheduler::Predicate;

    static constexpr Tid kExecuteInPool = std::numeric_limits<Tid>::max();

    virtual ~Dispatcher() = default;

    virtual std::optional<Tid> bind(std::shared_ptr<IScheduler> scheduler) = 0;
    virtual bool unbind(Tid tid) = 0;

    virtual void dispose() = 0;
    virtual void add(Tid tid, Task &&task) = 0;
    virtual void addDelayed(Tid tid,
                            std::chrono::microseconds timeout,
                            Task &&task) = 0;
    virtual void repeat(Tid tid,
                        std::chrono::microseconds timeout,
                        Task &&task,
                        Predicate &&pred) = 0;
  };

}  // namespace lean::se
