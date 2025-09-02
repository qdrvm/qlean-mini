/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include <boost/asio/ip/tcp.hpp>

namespace soralog {
  class Logger;
}  // namespace soralog

namespace lean::api {

  constexpr uint16_t kDefaultPortTolerance = 10;

  using Acceptor = boost::asio::ip::tcp::acceptor;
  using Endpoint = boost::asio::ip::tcp::endpoint;

  std::unique_ptr<Acceptor> acceptOnFreePort(
      std::shared_ptr<boost::asio::io_context> context,
      Endpoint endpoint,
      uint16_t port_tolerance,
      const std::shared_ptr<soralog::Logger> &logger);

}  // namespace lean::api
