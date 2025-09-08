/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#define VIRTUAL_DISPATCH(T) \
  virtual void dispatch_##T(std::shared_ptr<const messages::T> message) = 0
#define DISPATCH_OVERRIDE(T) \
  void dispatch_##T(std::shared_ptr<const messages::T> message) override

#define VIRTUAL_ON_DISPATCH(T) \
  virtual void on_dispatch_##T(std::shared_ptr<const messages::T> message) = 0
#define ON_DISPATCH_OVERRIDE(T) \
  void on_dispatch_##T(std::shared_ptr<const messages::T> message) override
#define ON_DISPATCH_IMPL(C, T) \
  void C::on_dispatch_##T(std::shared_ptr<const messages::T> message)


#define ON_DISPATCH_SUBSCRIPTION(T)                                       \
  std::shared_ptr<                                                        \
      BaseSubscriber<std::nullptr_t, std::shared_ptr<const messages::T>>> \
      on_dispatch_subscription_##T
#define ON_DISPATCH_SUBSCRIBE(T)                                      \
  on_dispatch_subscription_##T =                                      \
      se::SubscriberCreator<std::nullptr_t,                           \
                            std::shared_ptr<const messages::T>>::     \
          create(*se_manager_,                                        \
                 SubscriptionEngineHandlers::kTest,                   \
                 DeriveEventType::get<messages::T>(),                 \
                 [module_internal](                                   \
                     std::nullptr_t,                                  \
                     const std::shared_ptr<const messages::T> &msg) { \
                   if (auto m = module_internal.lock()) {             \
                     m->on_dispatch_##T(msg);                         \
                   }                                                  \
                 })
