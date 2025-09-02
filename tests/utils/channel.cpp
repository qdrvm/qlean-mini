/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "utils/channel.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <thread>

using namespace std::chrono_literals;
using namespace lean;

/**
 * @file channel.cpp
 * @brief Unit tests for Channel class covering send/receive behavior.
 */

#include <gtest/gtest.h>

#include <optional>
#include <thread>


/**
 * @brief Tests sending and receiving a single integer value through the
 * channel.
 * @details Creates a channel, sends the value 42, and verifies that the
 * receiver obtains it.
 */
TEST(ChannelTest, SendAndReceiveValue) {
  auto [recv, send] =
      Channel<int>::create_channel();  ///< Create a channel for int values.

  send.set(42);              ///< Send the integer value 42.
  auto value = recv.wait();  ///< Wait for and retrieve the sent value.

  ASSERT_TRUE(value.has_value());  ///< Verify that a value was received.
  EXPECT_EQ(value.value(),
            42);  ///< Check that the received value is equal to 42.
}

/**
 * @brief Tests sending an lvalue through the channel.
 * @details Sends a copy of the variable 'x' and ensures the receiver obtains
 * the correct value.
 */
TEST(ChannelTest, SendLValue) {
  auto [recv, send] = Channel<int>::create_channel();

  int x = 123;               ///< Define an integer variable.
  send.set(x);               ///< Send a copy of x.
  auto value = recv.wait();  ///< Receive the value.

  ASSERT_TRUE(value.has_value());  ///< Ensure a value was received.
  EXPECT_EQ(value.value(), 123);   ///< Validate that the value matches x.
}

/**
 * @brief Tests that destroying the sender notifies the receiver.
 * @details Starts a waiting thread on the receiver, destroys the sender, and
 * expects the receiver to unblock with no value.
 */
TEST(ChannelTest, SenderDestructionNotifiesReceiver) {
  std::optional<Channel<int>::Receiver> recv;
  std::optional<Channel<int>::Sender> send;

  std::tie(recv, send) = Channel<int>::create_channel();

  std::optional<int> result;

  std::thread t([&]() {
    result = recv->wait();
  });  ///< Thread blocks waiting for a value.

  std::this_thread::sleep_for(
      std::chrono::milliseconds(50));  ///< Ensure the thread is waiting.
  send.reset();  ///< Destroy the sender to signal end-of-transmission.

  t.join();  ///< Wait for the thread to finish.

  EXPECT_FALSE(result.has_value());  ///< The result should be empty since the
                                     ///< sender no longer exists.
}

/**
 * @brief Tests that multiple sends only allow one value to be received.
 * @details Sends two values consecutively; the receiver should get exactly one
 * of them.
 */
TEST(ChannelTest, MultipleSendKeepsOneValue) {
  auto [recv, send] = Channel<int>::create_channel();

  send.set(1);  ///< First send.
  send.set(2);  ///< Second send overrides or is ignored; only one value should
                ///< be kept.

  auto value = recv.wait();        ///< Receive the value.
  ASSERT_TRUE(value.has_value());  ///< Confirm a value was received.
  EXPECT_TRUE(value.value() == 1
              || value.value() == 2);  ///< Value should be either 1 or 2.
}

/**
 * @brief Tests that destroying the receiver unregisters it without throwing in
 * the sender.
 * @details Receiver is destroyed before sending; calling send.set() should not
 * throw an exception.
 */
TEST(ChannelTest, ReceiverDestructionUnregistersSender) {
  std::optional<Channel<int>::Receiver> recv;
  std::optional<Channel<int>::Sender> send;

  std::tie(recv, send) = Channel<int>::create_channel();

  recv.reset();  ///< Destroy the receiver prior to sending.

  EXPECT_NO_THROW(send->set(
      999));  ///< Sending after receiver destruction should not throw.
}
