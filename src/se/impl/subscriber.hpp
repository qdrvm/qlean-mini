/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include "common.hpp"
#include "utils/ctor_limiters.hpp"


namespace lean::se {

  using SubscriptionSetId = uint32_t;

  /**
   * Base implementation of subscription system's subscriber.
   * @tparam EventKey type to specify notified events
   * @tparam Dispatcher thread dispatcher to execute tasks
   * @tparam Arguments list of event arguments
   */
  template <typename EventKey, typename Dispatcher, typename... Arguments>
  class Subscriber : public std::enable_shared_from_this<
                         Subscriber<EventKey, Dispatcher, Arguments...>>,
                     NonCopyable,
                     NonMovable {
   public:
    // Default constructor
    Subscriber() = default;

    using EventType = EventKey;
    virtual ~Subscriber() = default;

    /**
     * Notification callback function
     * @param set_id the id of the subscription set
     * @param key notified event
     * @param args event data
     */
    virtual void on_notify(SubscriptionSetId set_id,
                           const EventType &key,
                           Arguments &&...args) = 0;
  };

}  // namespace lean::se
