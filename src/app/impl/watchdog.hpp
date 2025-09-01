/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>

#include <boost/asio/io_context.hpp>
#include <fmt/format.h>
#include <soralog/soralog.hpp>

#include "app/configuration.hpp"
#include "injector/dont_inject.hpp"
#include "log/logger.hpp"

namespace soralog {
  class Logger;
}  // namespace soralog

namespace lean::log {
  class LoggingSystem;
}  // namespace lean::log

#ifdef __APPLE__

#include <mach/mach.h>
#include <mach/thread_act.h>
#include <mach/thread_info.h>

namespace {
  inline uint64_t getPlatformThreadId() {
    thread_identifier_info_data_t info;
    mach_msg_type_number_t size = THREAD_IDENTIFIER_INFO_COUNT;
    auto r = thread_info(mach_thread_self(),
                         THREAD_IDENTIFIER_INFO,
                         (thread_info_t)&info,
                         &size);
    if (r != KERN_SUCCESS) {
      throw std::logic_error{"thread_info"};
    }
    return info.thread_id;
  }
}  // namespace

#else

#include <unistd.h>

#include <sys/syscall.h>

inline uint64_t getPlatformThreadId() {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
  return syscall(SYS_gettid);
}

#endif

namespace lean {

  constexpr auto kWatchdogDefaultTimeout = std::chrono::minutes{15};

  class Watchdog {
   public:
    using Count = uint32_t;
    using Atomic = std::atomic<Count>;
    using Clock = std::chrono::steady_clock;
    using Timeout = std::chrono::seconds;

    Watchdog(std::shared_ptr<log::LoggingSystem> logsys,
             std::shared_ptr<app::Configuration> config)
        : logger_(logsys->getLogger("Watchdog", "threads")),
          granularity_(1 /*config->granularity()*/) {
      // BOOST_ASSERT(granularity != granularity.zero());
    }

    struct Ping {
      std::shared_ptr<Atomic> count_;

      void operator()() const {
        count_->fetch_add(1);
      }
    };

    void checkLoop(Timeout timeout) {
      // or `io_context` with timer
      while (not stopped_) {
        std::this_thread::sleep_for(granularity_);
        check(timeout);
      }
    }

    // periodic check
    void check(const Timeout timeout) {
      std::unique_lock lock{mutex_};
      const auto now = Clock::now();
      for (auto it = threads_.begin(); it != threads_.end();) {
        if (it->second.count.use_count() == 1) {
          it = threads_.erase(it);
        } else {
          auto count = it->second.count->load();
          if (it->second.last_count != count) {
            it->second.last_count = count;
            it->second.last_time = now;
          } else {
            auto lag = now - it->second.last_time;
            if (lag > timeout) {
              std::stringstream s;
              s << it->first;
              SL_CRITICAL(logger_,
                          "ALERT Watchdog: "
                          "thread id={}, platform_id={}, name={}  timeout\n",
                          s.str(),
                          it->second.platform_id,
                          it->second.name);
              std::abort();
            }
          }
          ++it;
        }
      }
    }

    [[nodiscard]] Ping add() {
      std::unique_lock lock{mutex_};
      auto &thread = threads_[std::this_thread::get_id()];
      if (not thread.count) {
        thread = {.last_time = Clock::now(),
                  .last_count = 0,
                  .count = std::make_shared<Atomic>(),
                  .platform_id = getPlatformThreadId(),
                  .name = soralog::util::getThreadName()};
      }
      return Ping{thread.count};
    }

    void run(std::shared_ptr<boost::asio::io_context> io) {
      auto ping = add();
      while (not stopped_ and io.use_count() != 1) {
#define WAIT_FOR_BETTER_BOOST_IMPLEMENTATION
#ifndef WAIT_FOR_BETTER_BOOST_IMPLEMENTATION
        // this is the desired implementation
        // cannot be used rn due to the method visibility settings
        // bug(boost): wait_one is private
        boost::system::error_code ec;
        io->impl_.wait_one(TODO, ec);
        ping();
        io->poll_one();
#else
        // `run_one_for` run time is sum of `wait_one(time)` and `poll_one`
        // may cause false-positive timeout
        io->run_one_for(granularity_);
#endif
        ping();
        io->restart();
      }
    }

    void stop() {
      stopped_ = true;
    }

   private:
    struct Thread {
      Clock::time_point last_time;
      Count last_count = 0;
      std::shared_ptr<Atomic> count;
      uint64_t platform_id;
      std::string name;
    };

    std::shared_ptr<soralog::Logger> logger_;
    std::chrono::milliseconds granularity_;
    std::mutex mutex_;
    std::unordered_map<std::thread::id, Thread> threads_;
    std::atomic_bool stopped_ = false;
  };
}  // namespace lean
