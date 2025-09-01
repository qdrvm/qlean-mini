/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <assert.h>
#include <thread>

#include <soralog/util.hpp>
#include <fmt/format.h>
#include "scheduler_impl.hpp"

namespace lean::se {

  class ThreadHandler final : public SchedulerBase {
   private:
    std::thread worker_;

   public:
    ThreadHandler() {
      worker_ = std::thread(
          [](ThreadHandler *__this) {
            static std::atomic_size_t counter = 0;
            auto tname = fmt::format("worker.{}", ++counter);
            soralog::util::setThreadName(tname);
            return __this->process();
          }, this);
    }

    void dispose(bool wait_for_release = true) {
      SchedulerBase::dispose(wait_for_release);
      if (wait_for_release) {
        worker_.join();
      } else {
        worker_.detach();
      }
    }
  };

}  // namespace lean::se
