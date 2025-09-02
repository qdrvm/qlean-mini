/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <stdexcept>

#include <fmt/format.h>

namespace lean {

  class NonCopyable {
   public:
    // To prevent copy of instance
    NonCopyable() = default;
    ~NonCopyable() = default;
    NonCopyable(const NonCopyable &) = delete;
    NonCopyable &operator=(const NonCopyable &) = delete;
    NonCopyable(NonCopyable &&) = default;
    NonCopyable &operator=(NonCopyable &&) = default;
  };

  class NonMovable {
   public:
    // To prevent movement of instance
    NonMovable() = default;
    ~NonMovable() = default;
    NonMovable(NonMovable &&) = delete;
    NonMovable &operator=(NonMovable &&) = delete;
    NonMovable(const NonMovable &) = default;
    NonMovable &operator=(const NonMovable &) = default;
  };

  class StackOnly {
   public:
    // To prevent an object being created on the heap
    void *operator new(std::size_t) = delete;            // standard new
    void *operator new(std::size_t, void *) = delete;    // placement new
    void *operator new[](std::size_t) = delete;          // array new
    void *operator new[](std::size_t, void *) = delete;  // placement array new
  };

  class HeapOnly {
   protected:
    ~HeapOnly() = default;
  };

  template <typename T>
    requires std::same_as<T, std::decay_t<T>>
  class Singleton : NonCopyable, NonMovable {
    using BaseType = T;

   public:
    Singleton() {
      if (exists.test_and_set(std::memory_order_acquire)) {
        throw std::logic_error(
            fmt::format("Attempt to create one more instance of singleton '{}'",
                        typeid(BaseType).name()));
      }
    }
    ~Singleton() {
      exists.clear(std::memory_order_release);
    }

   private:
    inline static std::atomic_flag exists{false};
  };

}  // namespace lean
