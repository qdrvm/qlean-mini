/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include "impl/common.hpp"
#include "impl/subscriber_impl.hpp"
#include "impl/subscription_manager.hpp"
#include "subscription_fwd.hpp"

namespace lean::se {
  std::shared_ptr<Dispatcher> getDispatcher();

  template <typename ObjectType, typename... EventData>
  struct SubscriberCreator {
    template <EventTypes key, typename F, typename... Args>
    static auto create(Subscription &se,
                       SubscriptionEngineHandlers tid,
                       F &&callback,
                       Args &&...args) {
      auto subscriber = BaseSubscriber<ObjectType, EventData...>::create(
          se.getEngine<EventTypes, EventData...>(),
          std::forward<Args>(args)...);
      subscriber->setCallback(
          [f{std::forward<F>(callback)}](auto /*set_id*/,
                                         auto &object,
                                         auto event_key,
                                         EventData... args) mutable {
            assert(key == event_key);
            std::forward<F>(f)(object, std::move(args)...);
          });
      subscriber->subscribe(0, key, static_cast<Dispatcher::Tid>(tid));
      return subscriber;
    }
  };
}  // namespace lean::se
