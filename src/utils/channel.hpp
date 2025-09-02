/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <type_traits>

#include "utils/ctor_limiters.hpp"

namespace lean {

  /**
   * @brief A generic communication channel between two endpoints
   *
   * Channel provides a type-safe way to send values from a sender to a
   * receiver. The implementation ensures thread-safety and proper
   * synchronization between the sender and receiver endpoints.
   *
   * @tparam T The type of data that will be transmitted through the channel
   */
  template <typename T>
  struct Channel {
    struct _Receiver;
    struct _Sender;

    struct _Receiver {
      using Other = _Sender;
    };
    struct _Sender {
      using Other = _Receiver;
    };

    /**
     * @brief Base template for sender and receiver endpoints
     *
     * This template defines the common functionality for both sender and
     * receiver endpoints of a channel. The behavior is specialized based on the
     * Opp template parameter to create either a sender or receiver endpoint.
     *
     * @tparam Opp The opposite endpoint type (_Sender or _Receiver)
     */
    template <typename Opp>
    struct Endpoint : NonCopyable {
      static_assert(std::is_same_v<Opp, _Receiver>
                        || std::is_same_v<Opp, _Sender>,
                    "Incorrect type");
      static constexpr bool IsReceiver = std::is_same_v<Opp, _Receiver>;
      static constexpr bool IsSender = std::is_same_v<Opp, _Sender>;

      /**
       * @brief Move constructor for receiver endpoint
       *
       * Transfers the connection from another receiver endpoint to this one.
       *
       * @param other The receiver endpoint to move from
       */
      Endpoint(Endpoint &&other)
        requires(IsReceiver)
      {
        context_.exclusiveAccess([&](auto &my_context) {
          Endpoint<typename Opp::Other> *opp = nullptr;
          while (other.context_.exclusiveAccess([&](auto &other_context) {
            if (other_context.opp_) {
              if (!other_context.opp_->register_opp(*this)) {
                return true;
              }
              opp = other_context.opp_;
              other_context.opp_ = nullptr;
            }
            return false;
          }));
          my_context.opp_ = opp;
        });
      }

      /**
       * @brief Move constructor for sender endpoint
       *
       * Transfers the connection from another sender endpoint to this one.
       *
       * @param other The sender endpoint to move from
       */
      Endpoint(Endpoint &&other)
        requires(IsSender)
      {
        context_.exclusiveAccess([&](auto &my_context) {
          my_context.opp_ =
              other.context_.exclusiveAccess([&](auto &other_context) {
                Endpoint<typename Opp::Other> *opp = nullptr;
                if (other_context.opp_) {
                  other_context.opp_->register_opp(*this);
                  opp = other_context.opp_;
                  other_context.opp_ = nullptr;
                }
                return opp;
              });
        });
      }

      /**
       * @brief Move assignment operator for receiver endpoint
       *
       * Transfers the connection from another receiver endpoint to this one.
       *
       * @param other The receiver endpoint to move from
       * @return Reference to this endpoint
       */
      Endpoint &operator=(Endpoint &&other)
        requires(IsReceiver)
      {
        if (this != &other) {
          context_.exclusiveAccess([&](auto &my_context) {
            Endpoint<typename Opp::Other> *opp = nullptr;
            while (other.context_.exclusiveAccess([&](auto &other_context) {
              if (other_context.opp_) {
                if (!other_context.opp_->register_opp(*this)) {
                  return true;
                }
                opp = other_context.opp_;
                other_context.opp_ = nullptr;
              }
              return false;
            }));
            my_context.opp_ = opp;
          });
        }
        return *this;
      }

      /**
       * @brief Move assignment operator for sender endpoint
       *
       * Transfers the connection from another sender endpoint to this one.
       *
       * @param other The sender endpoint to move from
       * @return Reference to this endpoint
       */
      Endpoint &operator=(Endpoint &&other)
        requires(IsSender)
      {
        if (this != &other) {
          context_.exclusiveAccess([&](auto &my_context) {
            my_context.opp_ =
                other.context_.exclusiveAccess([&](auto &other_context) {
                  Endpoint<typename Opp::Other> *opp = nullptr;
                  if (other_context.opp_) {
                    other_context.opp_->register_opp(*this);
                    opp = other_context.opp_;
                    other_context.opp_ = nullptr;
                  }
                  return opp;
                });
          });
        }
        return *this;
      }

      /**
       * @brief Registers the opposite endpoint for a receiver
       *
       * Links this receiver endpoint with the provided sender endpoint.
       *
       * @param opp The sender endpoint to link with
       * @return true if registration was successful
       */
      bool register_opp(Endpoint<typename Opp::Other> &opp)
        requires(IsReceiver)
      {
        return context_.exclusiveAccess([&](auto &context) {
          context.opp_ = &opp;
          return true;
        });
      }

      /**
       * @brief Registers the opposite endpoint for a sender
       *
       * Links this sender endpoint with the provided receiver endpoint.
       *
       * @param opp The receiver endpoint to link with
       * @return true if registration was successful
       */
      bool register_opp(Endpoint<typename Opp::Other> &opp)
        requires(IsSender)
      {
        return context_
            .try_exclusiveAccess([&](auto &context) { context.opp_ = &opp; })
            .has_value();
      }

      /**
       * @brief Unregisters the opposite endpoint for a receiver
       *
       * Unlinks this receiver endpoint from the provided sender endpoint.
       *
       * @param opp The sender endpoint to unlink from
       * @return true if unregistration was successful
       */
      bool unregister_opp(Endpoint<typename Opp::Other> &opp)
        requires(IsReceiver)
      {
        return context_.exclusiveAccess([&](auto &context) {
          assert(context.opp_ == &opp);
          context.opp_ = nullptr;
          return true;
        });
      }

      /**
       * @brief Unregisters the opposite endpoint for a sender
       *
       * Unlinks this sender endpoint from the provided receiver endpoint.
       *
       * @param opp The receiver endpoint to unlink from
       * @return true if unregistration was successful
       */
      bool unregister_opp(Endpoint<typename Opp::Other> &opp)
        requires(IsSender)
      {
        return context_
            .try_exclusiveAccess([&](auto &context) {
              assert(context.opp_ == &opp);
              context.opp_ = nullptr;
            })
            .has_value();
      }

      /**
       * @brief Destructor for sender endpoint
       *
       * Cleans up the sender endpoint and signals the receiver if needed.
       */
      ~Endpoint()
        requires(IsSender)
      {
        context_.exclusiveAccess([&](auto &context) {
          if (context.opp_) {
            context.opp_->unregister_opp(*this);
            context.opp_->event_.set();
            context.opp_ = nullptr;
          }
        });
      }


      /**
       * @brief Destructor for receiver endpoint
       *
       * Cleans up the receiver endpoint and ensures proper unregistration.
       */
      ~Endpoint()
        requires(IsReceiver)
      {
        while (context_.exclusiveAccess([&](auto &context) {
          if (context.opp_) {
            if (!context.opp_->unregister_opp(*this)) {
              return true;
            }
            context.opp_ = nullptr;
          }
          return false;
        }));
      }

      /**
       * @brief Sends a value through the channel by moving it
       *
       * This method sends the provided value to the receiver endpoint.
       *
       * @param t The value to send (will be moved)
       */
      void set(T &&t)
        requires(IsSender)
      {
        context_.exclusiveAccess([&](auto &context) {
          if (context.opp_) {
            context.opp_->context_.exclusiveAccess(
                [&](auto &c) { c.data_ = std::move(t); });
            context.opp_->event_.set();
          }
        });
      }

      /**
       * @brief Sends a value through the channel by copying it
       *
       * This method sends the provided value to the receiver endpoint.
       *
       * @param t The value to send (will be copied)
       */
      void set(T &t)
        requires(IsSender)
      {
        context_.exclusiveAccess([&](auto &context) {
          if (context.opp_) {
            context.opp_->context_.exclusiveAccess(
                [&](auto &c) { c.data_ = t; });
            context.opp_->event_.set();
          }
        });
      }

      /**
       * @brief Waits for and receives a value from the channel
       *
       * This method blocks until a value is available in the channel,
       * then returns that value.
       *
       * @return The value received from the channel, or nullopt if no value is
       * available
       */
      std::optional<T> wait()
        requires(IsReceiver)
      {
        event_.wait();
        return context_.exclusiveAccess(
            [&](auto &context) { return std::move(context.data_); });
      }

     private:
      friend struct Endpoint<typename Opp::Other>;
      /**
       * @brief Internal context structure to safely store endpoint state
       */
      struct SafeContext {
        std::conditional_t<std::is_same_v<Opp, _Receiver>,
                           std::optional<T>,
                           std::monostate>
            data_;
        Endpoint<typename Opp::Other> *opp_ = nullptr;
      };

      std::conditional_t<std::is_same_v<Opp, _Receiver>,
                         lean::se::utils::WaitForSingleObject,
                         std::monostate>
          event_;
      lean::se::utils::SafeObject<SafeContext, std::mutex> context_;
    };

    using Receiver = Endpoint<_Receiver>;
    using Sender = Endpoint<_Sender>;

    /**
     * @brief Creates a new channel with connected sender and receiver endpoints
     *
     * This function creates and returns a new channel with properly connected
     * sender and receiver endpoints.
     *
     * @return A pair containing the receiver and sender endpoints
     */
    inline std::pair<Receiver, Sender> create_channel() {
      using C = Channel<T>;
      C::Receiver r;
      C::Sender s;

      r.register_opp(s);
      s.register_opp(r);
      return std::make_pair(std::move(r), std::move(s));
    }
  };

}  // namespace lean
