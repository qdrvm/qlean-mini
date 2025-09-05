/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include <qtils/empty.hpp>

#include "impl/common.hpp"
#include "impl/subscriber_impl.hpp"
#include "impl/subscription_manager.hpp"
#include "subscription_fwd.hpp"

/**
 * @file
 * @brief Subscriber factory helper for the Subscription Engine.
 *
 * This header provides a tiny convenience layer to create and configure
 * typed subscribers bound to a particular @ref Subscription instance.
 * It wires a user callback and subscribes to a specific event key using the
 * engine's dispatcher thread id.
 */

/**
 * Principles of operation
 * -----------------------
 * - A @ref BaseSubscriber is created for the target @ref Subscription engine
 *   using the provided @c ContextType and @c EventData... types.
 * - A user callback is installed; it will be invoked with a reference to the
 *   context object and the event payload (expanded from @c EventData...).
 * - The subscriber is subscribed to a concrete @ref EventTypes key using the
 *   supplied dispatcher thread identifier (@ref Dispatcher::Tid).
 * - Event routing asserts that the runtime key equals the compile‑time key
 *   specified at the call site of @c create.
 */

namespace lean::se {
  /**
   * @brief Obtain a shared dispatcher instance.
   *
   * @return Shared pointer to a @ref Dispatcher used by the Subscription
   *         Engine to route events.
   */
  std::shared_ptr<Dispatcher> getDispatcher();

  /**
   * @tparam ContextType  Type of the per-subscriber context object passed to
   *                      callbacks. Can be set into `qtils::Empty` if it is
   *                      unnecessary
   * @tparam EventData    Variadic list of event payload types delivered to the
   *                      callback in the declared order.
   */
  template <typename ContextType, typename... EventData>
  struct SubscriberCreator {
    /**
     * @brief Create and subscribe a typed subscriber with a user callback.
     *
     * Creates a @ref BaseSubscriber bound to the engine taken from the given
     * @ref Subscription, installs the callback, and subscribes it to the
     * specified event @p key on the provided dispatcher thread id @p tid.
     *
     * @tparam key   Compile‑time event key (an enumerator of @ref EventTypes)
     *               this subscriber should receive.
     * @tparam F     Callable type of the user callback. The callback must be
     *               invocable with arguments: (ContextType&, EventData...).
     * @tparam Args  Additional arguments forwarded to the underlying
     *               @c BaseSubscriber::create factory (e.g., context args).
     *
     * @param se       Reference to the owning @ref Subscription.
     * @param tid      Dispatcher thread identifier to bind the subscription to.
     * @param callback User callback invoked for delivered events.
     * @param args     Extra arguments forwarded to subscriber construction.
     *
     * @return A @c std::shared_ptr to the created @ref BaseSubscriber.
     */
    template <EventTypes key, typename F, typename... Args>
    static auto create(Subscription &se,
                       SubscriptionEngineHandlers tid,
                       F &&callback,
                       Args &&...args) {
      auto subscriber = BaseSubscriber<ContextType, EventData...>::create(
          se.getEngine<EventTypes, EventData...>(),
          std::forward<Args>(args)...);
      subscriber->setCallback(
          [f{std::forward<F>(callback)}](auto /*set_id*/,
                                         auto &context,
                                         auto event_key,
                                         EventData... args) mutable {
            assert(key == event_key);
            std::forward<F>(f)(context, std::move(args)...);
          });
      subscriber->subscribe(0, key, static_cast<Dispatcher::Tid>(tid));
      return subscriber;
    }
  };
}  // namespace lean::se
