/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>

#include "utils/ctor_limiters.hpp"

namespace lean::se::utils {

  /**
   * @brief Creates a weak_ptr from a shared_ptr
   *
   * Utility function that simplifies creating a weak_ptr from a shared_ptr.
   *
   * @tparam T The type of object pointed to
   * @param ptr The shared_ptr to create a weak_ptr from
   * @return A weak_ptr that shares ownership with the input shared_ptr
   */
  template <typename T>
  inline std::weak_ptr<T> make_weak(const std::shared_ptr<T> &ptr) noexcept {
    return ptr;
  }

  /**
   * @brief Thread-safe wrapper for any object
   *
   * SafeObject provides exclusive and shared access patterns to an internal
   * object with proper synchronization. It uses a mutex to protect the object
   * from concurrent access.
   *
   * @tparam T The type of object to wrap
   * @tparam M The mutex type to use for synchronization (defaults to
   * std::shared_mutex)
   */
  template <typename T, typename M = std::shared_mutex>
  struct SafeObject {
    using Type = T;

    /**
     * @brief Constructor that forwards arguments to the wrapped object
     *
     * @tparam Args Parameter pack of argument types
     * @param args Arguments to forward to the wrapped object's constructor
     */
    template <typename... Args>
    SafeObject(Args &&...args) : t_(std::forward<Args>(args)...) {}

    /**
     * @brief Provides exclusive (write) access to the wrapped object
     *
     * Locks the mutex to ensure exclusive access and applies the provided
     * function to the wrapped object.
     *
     * @tparam F Type of the function to apply
     * @param f Function to apply to the wrapped object
     * @return The result of applying the function to the wrapped object
     */
    template <typename F>
    inline auto exclusiveAccess(F &&f) {
      std::unique_lock lock(cs_);
      return std::forward<F>(f)(t_);
    }

    /**
     * @brief Attempts to get exclusive access without blocking
     *
     * Tries to lock the mutex. If successful, applies the provided function to
     * the wrapped object and returns the result. If unsuccessful, returns an
     * empty optional.
     *
     * @tparam F Type of the function to apply
     * @param f Function to apply to the wrapped object
     * @return An optional containing the result of the function, or empty if
     * the lock was not acquired
     */
    template <typename F>
    inline auto try_exclusiveAccess(F &&f) {
      std::unique_lock lock(cs_, std::try_to_lock);
      using ResultType = decltype(std::forward<F>(f)(t_));
      constexpr bool is_void = std::is_void_v<ResultType>;
      using OptionalType = std::conditional_t<is_void,
                                              std::optional<std::monostate>,
                                              std::optional<ResultType>>;

      if (lock.owns_lock()) {
        if constexpr (is_void) {
          std::forward<F>(f)(t_);
          return OptionalType(std::in_place);
        } else {
          return OptionalType(std::forward<F>(f)(t_));
        }
      } else {
        return OptionalType();
      }
    }

    /**
     * @brief Provides shared (read) access to the wrapped object
     *
     * Acquires a shared lock on the mutex and applies the provided function
     * to the wrapped object.
     *
     * @tparam F Type of the function to apply
     * @param f Function to apply to the wrapped object
     * @return The result of applying the function to the wrapped object
     */
    template <typename F>
    inline auto sharedAccess(F &&f) const {
      std::shared_lock lock(cs_);
      return std::forward<F>(f)(t_);
    }

    T &unsafeGet() {
      return t_;
    }

    const T &unsafeGet() const {
      return t_;
    }

   private:
    T t_;           ///< The wrapped object
    mutable M cs_;  ///< Mutex for synchronization
  };

  /**
   * @brief Alias for SafeObject with a more descriptive name
   *
   * Provides the same functionality as SafeObject but with a name that better
   * describes the read-write access pattern.
   *
   * @tparam T The type of object to wrap
   * @tparam M The mutex type to use for synchronization (defaults to
   * std::shared_mutex)
   */
  template <typename T, typename M = std::shared_mutex>
  using ReadWriteObject = SafeObject<T, M>;

  /**
   * @brief A synchronization primitive similar to a manual reset event
   *
   * WaitForSingleObject provides a way to signal and wait for a condition
   * between threads. It's similar to a manual reset event, where one thread
   * can wait until another thread signals the event.
   */
  class WaitForSingleObject final : NonCopyable, NonMovable {
    std::condition_variable wait_cv_;  ///< Condition variable for waiting
    std::mutex wait_m_;                ///< Mutex for synchronization
    bool flag_;  ///< Flag that represents the state (true = not signaled, false
                 ///< = signaled)

   public:
    /**
     * @brief Constructor that initializes the object in the not signaled state
     */
    WaitForSingleObject() : flag_{true} {}

    /**
     * @brief Waits for the object to be signaled with a timeout
     *
     * Blocks the current thread until the object is signaled or the timeout
     * expires. The state is automatically reset to not signaled after a
     * successful wait.
     *
     * @param wait_timeout Maximum time to wait
     * @return true if the object was signaled, false if the timeout expired
     */
    bool wait(std::chrono::microseconds wait_timeout) {
      std::unique_lock<std::mutex> _lock(wait_m_);
      return wait_cv_.wait_for(_lock, wait_timeout, [&]() {
        auto prev = !flag_;
        flag_ = true;
        return prev;
      });
    }

    /**
     * @brief Waits indefinitely for the object to be signaled
     *
     * Blocks the current thread until the object is signaled.
     * The state is automatically reset to not signaled after a successful wait.
     */
    void wait() {
      std::unique_lock<std::mutex> _lock(wait_m_);
      wait_cv_.wait(_lock, [&]() {
        auto prev = !flag_;
        flag_ = true;
        return prev;
      });
    }

    /**
     * @brief Signals the object
     *
     * Sets the object to the signaled state and wakes one waiting thread.
     */
    void set() {
      {
        std::unique_lock<std::mutex> _lock(wait_m_);
        flag_ = false;
      }
      wait_cv_.notify_one();
    }
  };
}  // namespace lean::se::utils
