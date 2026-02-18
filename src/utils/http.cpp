/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "utils/http.hpp"

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>
#include <libp2p/coro/asio.hpp>
#include <libp2p/coro/spawn.hpp>

namespace lean::http {
  inline libp2p::Coro<void> serve(log::Logger log,
                                  boost::asio::ip::tcp::socket socket,
                                  ServerConfig config) {
    boost::beast::tcp_stream stream{std::move(socket)};
    boost::beast::flat_buffer buffer;
    Request request;
    auto read_res = libp2p::coroOutcome(co_await boost::beast::http::async_read(
        stream, buffer, request, libp2p::useCoroOutcome));
    if (not read_res.has_value()) {
      SL_WARN(log, "http read request error: {}", read_res.error());
      co_return;
    }
    auto response = config.on_request(std::move(request));
    auto write_res =
        libp2p::coroOutcome(co_await boost::beast::http::async_write(
            stream, response, libp2p::useCoroOutcome));
    if (not write_res.has_value()) {
      SL_WARN(log, "http write response error: {}", write_res.error());
    }
  }

  outcome::result<void> serve(log::Logger log,
                              boost::asio::io_context &io_context,
                              ServerConfig config) {
    boost::asio::ip::tcp::acceptor acceptor{io_context};
    boost::system::error_code ec;
    acceptor.open(config.endpoint.protocol(), ec);
    if (ec) {
      return ec;
    }
    acceptor.bind(config.endpoint, ec);
    if (ec) {
      return ec;
    }
    acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
      return ec;
    }
    libp2p::coroSpawn(
        io_context,
        [log, &io_context, config, acceptor{std::move(acceptor)}]() mutable
            -> libp2p::Coro<void> {
          while (true) {
            auto accept_res = libp2p::coroOutcome(
                co_await acceptor.async_accept(libp2p::useCoroOutcome));
            if (not accept_res.has_value()) {
              SL_WARN(log, "tcp accept error: {}", accept_res.error());
              break;
            }
            auto &socket = accept_res.value();
            libp2p::coroSpawn(io_context,
                              [log, config, socket{std::move(socket)}]() mutable
                                  -> libp2p::Coro<void> {
                                co_await serve(log, std::move(socket), config);
                              });
          }
        });
    return outcome::success();
  }
}  // namespace lean::http
