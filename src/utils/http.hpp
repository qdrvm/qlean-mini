/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <functional>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http/message_fwd.hpp>
#include <boost/beast/http/string_body_fwd.hpp>

#include "log/logger.hpp"

namespace boost::asio {
  class io_context;
}  // namespace boost::asio

namespace lean::http {
  using Body = boost::beast::http::string_body;
  using Request = boost::beast::http::request<Body>;
  using Response = boost::beast::http::response<Body>;
  using OnRequest = std::function<Response(Request)>;

  struct ServerConfig {
    boost::asio::ip::tcp::endpoint endpoint;
    OnRequest on_request;

    static constexpr size_t kDefaultRequestSize = 10000u;
    size_t max_request_size{kDefaultRequestSize};

    using Duration = std::chrono::nanoseconds;
    static constexpr Duration kDefaultTimeout = std::chrono::seconds{30};
    Duration operation_timeout{kDefaultTimeout};
  };

  outcome::result<void> serve(log::Logger log,
                              boost::asio::io_context &io_context,
                              ServerConfig config);
}  // namespace lean::http
