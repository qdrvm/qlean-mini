/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <utility>

namespace lean {
  enum class SubscriptionEngineHandlers {
    kTest = 0,
    //---------------
    kTotalCount
  };

  static constexpr uint32_t kHandlersCount =
      static_cast<uint32_t>(SubscriptionEngineHandlers::kTotalCount);

  enum class EventTypes {
    // -- Modules

    /// All available modules are loaded
    LoadingIsFinished,

    // -- Example

    /// Example module is loaded
    ExampleModuleIsLoaded,
    /// Example module is unloaded
    ExampleModuleIsUnloaded,
    /// Example notification
    ExampleNotification,
    /// Example request
    ExampleRequest,
    /// Example response
    ExampleResponse,

    // -- Networking

    /// Networking module is loaded
    NetworkingIsLoaded,
    /// Networking module is unloaded
    NetworkingIsUnloaded,
    /// Peer connected
    PeerConnected,
    /// Peer disconnected
    PeerDisconnected,
    /// Peer counts by user-agent is updated
    PeerCountsUpdated,
    /// Data of a block is requested
    BlockRequest,
    /// Data of a block is respond
    BlockResponse,

    // -- Block production

    /// Production module is loaded
    ProductionIsLoaded,
    /// Production module is unloaded
    ProductionIsUnloaded,
    /// New block produced
    BlockProduced,

    // -- Synchronizer

    /// Synchronizer module is loaded
    SynchronizerIsLoaded,
    /// Synchronizer module is unloaded
    SynchronizerIsUnloaded,
    /// Block announce received
    BlockAnnounceReceived,
    /// New block index discovered
    BlockIndexDiscovered,

    // -- Block tree

    /// New leaf
    BlockAdded,
    /// Finalized
    BlockFinalized,

    // -- Timeline

    /// New slot started
    SlotStarted,
    SlotIntervalStarted,

    /// Used by `DeriveEventType::get`
    Derive,
  };

  /**
   * Get `EventType` auto-assigned to type `T`.
   */
  class DeriveEventType {
   public:
    template <typename T>
    static EventTypes get() {
      static auto type = static_cast<EventTypes>(
          std::to_underlying(EventTypes::Derive) + nextIndex());
      return type;
    }

   private:
    static size_t nextIndex() {
      static size_t index = 0;
      return index++;
    }
  };

  /**
   * Call `notify` with `EventType` auto-assigned to type `T`.
   */
  template <typename T>
  void dispatchDerive(auto &subscription, const std::shared_ptr<T> &message) {
    subscription.notify(DeriveEventType::get<std::remove_cvref_t<T>>(),
                        message);
  }

  static constexpr uint32_t kThreadPoolSize = 3u;

  namespace se {
    struct Dispatcher;

    template <uint32_t kHandlersCount, uint32_t kPoolSize>
    class SubscriptionManager;

    template <typename EventKey,
              typename Dispatcher,
              typename Receiver,
              typename... Arguments>
    class SubscriberImpl;
  }  // namespace se

  using Dispatcher = se::Dispatcher;
  using Subscription = se::SubscriptionManager<kHandlersCount, kThreadPoolSize>;
  template <typename ContextType, typename... EventData>
  using BaseSubscriber =
      se::SubscriberImpl<EventTypes, Dispatcher, ContextType, EventData...>;

}  // namespace lean
