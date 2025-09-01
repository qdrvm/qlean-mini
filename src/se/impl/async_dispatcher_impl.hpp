/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "common.hpp"
#include "dispatcher.hpp"
#include "thread_handler.hpp"
#include "utils/ctor_limiters.hpp"

namespace lean::se {

  template <uint32_t kCount, uint32_t kPoolSize>
  class AsyncDispatcher final : public Dispatcher, NonCopyable, NonMovable {
   public:
    static constexpr uint32_t kHandlersCount = kCount;
    static constexpr uint32_t kPoolThreadsCount = kPoolSize;

   private:
    using Parent = Dispatcher;

    struct SchedulerContext {
      /// Scheduler to execute tasks
      std::shared_ptr<IScheduler> handler;
    };

    SchedulerContext handlers_[kHandlersCount];
    SchedulerContext pool_[kPoolThreadsCount];

    std::atomic_int64_t temporary_handlers_tasks_counter_;
    std::atomic<bool> is_disposed_;

    struct BoundContexts {
      typename Parent::Tid next_tid_offset = 0u;
      std::unordered_map<typename Parent::Tid, SchedulerContext> contexts;
    };
    utils::ReadWriteObject<BoundContexts> bound_;

    void uploadToHandler(const typename Parent::Tid tid,
                         std::chrono::microseconds timeout,
                         typename Parent::Task &&task,
                         typename Parent::Predicate &&pred) {
      assert(tid != kExecuteInPool || !pred);
      if (is_disposed_.load()) {
        return;
      }

      if (tid < kHandlersCount) {
        pred ? handlers_[tid].handler->repeat(
                   timeout, std::move(task), std::move(pred))
             : handlers_[tid].handler->addDelayed(timeout, std::move(task));
        return;
      }

      if (auto context =
              bound_.sharedAccess([tid](const BoundContexts &bound)
                                      -> std::optional<SchedulerContext> {
                if (auto it = bound.contexts.find(tid);
                    it != bound.contexts.end()) {
                  return it->second;
                }
                return std::nullopt;
              })) {
        pred ? context->handler->repeat(
                   timeout, std::move(task), std::move(pred))
             : context->handler->addDelayed(timeout, std::move(task));
        return;
      }

      std::optional<typename Parent::Task> opt_task = std::move(task);
      for (auto &handler : pool_) {
        if (opt_task =
                handler.handler->uploadIfFree(timeout, std::move(*opt_task));
            !opt_task) {
          return;
        }
      }

      auto h = std::make_shared<ThreadHandler>();
      ++temporary_handlers_tasks_counter_;
      h->addDelayed(timeout, [this, h, task{std::move(*opt_task)}]() mutable {
        if (!is_disposed_.load()) {
          task();
        }
        --temporary_handlers_tasks_counter_;
        h->dispose(false);
      });
    }

   public:
    AsyncDispatcher() {
      temporary_handlers_tasks_counter_.store(0);
      is_disposed_ = false;
      for (auto &h : handlers_) {
        h.handler = std::make_shared<ThreadHandler>();
      }
      for (auto &h : pool_) {
        h.handler = std::make_shared<ThreadHandler>();
      }
    }

    void dispose() override {
      is_disposed_ = true;
      for (auto &h : handlers_) {
        h.handler->dispose();
      }
      for (auto &h : pool_) {
        h.handler->dispose();
      }

      while (temporary_handlers_tasks_counter_.load() != 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(0ull));
      }
    }

    void add(typename Parent::Tid tid, typename Parent::Task &&task) override {
      uploadToHandler(
          tid, std::chrono::microseconds(0ull), std::move(task), nullptr);
    }

    void addDelayed(typename Parent::Tid tid,
                    std::chrono::microseconds timeout,
                    typename Parent::Task &&task) override {
      uploadToHandler(tid, timeout, std::move(task), nullptr);
    }

    void repeat(typename Parent::Tid tid,
                std::chrono::microseconds timeout,
                typename Parent::Task &&task,
                typename Parent::Predicate &&pred) override {
      uploadToHandler(tid, timeout, std::move(task), std::move(pred));
    }

    std::optional<Tid> bind(std::shared_ptr<IScheduler> scheduler) override {
      if (!scheduler) {
        return std::nullopt;
      }

      return bound_.exclusiveAccess(
          [scheduler(std::move(scheduler))](BoundContexts &bound) {
            const auto execution_tid = kHandlersCount + bound.next_tid_offset;
            assert(bound.contexts.find(execution_tid) == bound.contexts.end());
            bound.contexts[execution_tid] = SchedulerContext{scheduler};
            ++bound.next_tid_offset;
            return execution_tid;
          });
    }

    bool unbind(Tid tid) override {
      return bound_.exclusiveAccess([tid](BoundContexts &bound) {
        return bound.contexts.erase(tid) == 1;
      });
    }
  };

}  // namespace lean::se
