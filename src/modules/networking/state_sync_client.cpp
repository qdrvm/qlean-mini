/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */


#include "modules/networking/state_sync_client.hpp"

#include <future>
#include <memory>
#include <type_traits>
#include <utility>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/url.hpp>
#include <openssl/ssl.h>
#include <qtils/final_action.hpp>

#include "app/state_manager.hpp"
#include "modules/networking/ssl_context.hpp"

namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;
using namespace std::chrono_literals;

OUTCOME_CPP_DEFINE_CATEGORY(lean, StateSyncError, e) {
  using E = StateSyncError;
  switch (e) {
    case E::Busy:
      return "client is busy";
    case E::Timeout:
      return "request timed out";
    case E::ShuttingDown:
      return "node is shutting down";
    case E::InvalidUrl:
      return "invalid URL";
    case E::UnsupportedScheme:
      return "unsupported URL scheme";
    case E::UserInfoIsNotSupported:
      return "user-info is not supported";
    case E::Network:
      return "network error";
    case E::HttpBadStatus:
      return "HTTP bad status";
    case E::DeserializeFailed:
      return "deserialization failed";
    case E::ValidationFailed:
      return "validation failed";
  }
  return "Unknown error";
}

namespace lean {

  namespace {

    struct UrlParts {
      std::string scheme;  // "http" or "https"
      std::string host;    // hostname without port
      std::string port;    // explicit or default (80/443)
      std::string target;  // path + ("?" + query) if present
    };

    outcome::result<UrlParts> parse_url(const std::string &url) {
      namespace urls = boost::urls;

      // We accept absolute URIs like: http(s)://host[:port]/path?query
      const auto r = urls::parse_uri(url);
      if (not r) {
        return StateSyncError::InvalidUrl;
      }

      const urls::url_view uv = r.value();

      if (uv.scheme() != "http" and uv.scheme() != "https") {
        return StateSyncError::UnsupportedScheme;
      }

      if (not uv.has_authority()) {
        return StateSyncError::InvalidUrl;
      }

      if (!uv.userinfo().empty()) {
        return StateSyncError::UserInfoIsNotSupported;
      }

      if (uv.host().empty()) {
        return StateSyncError::InvalidUrl;
      }

      std::string port;
      if (uv.has_port()) {
        port = uv.port();
      } else {
        port = uv.scheme() == "https" ? "443" : "80";
      }

      // Beast wants target = path[?query] (encoded).
      std::string target;
      if (uv.encoded_path().empty()) {
        target = "/";
      } else {
        target = uv.encoded_path();
      }
      if (uv.has_query()) {
        target.push_back('?');
        target += uv.encoded_query();
      }

      return UrlParts{
          .scheme = uv.scheme(),
          .host = uv.host(),
          .port = std::move(port),
          .target = std::move(target),
      };
    }

    enum class CancelReason : uint8_t {
      None = 0,
      Timeout,
      ShuttingDown,
    };

    struct CancelState {
      std::atomic<CancelReason> reason{CancelReason::None};

      void set_if_none(CancelReason r) {
        auto expected = CancelReason::None;
        (void)reason.compare_exchange_strong(
            expected, r, std::memory_order_acq_rel, std::memory_order_relaxed);
      }
    };

    outcome::result<State> map_aborted(const CancelState &cs) {
      const auto r = cs.reason.load(std::memory_order_acquire);
      if (r == CancelReason::Timeout) {
        return StateSyncError::Timeout;
      }
      if (r == CancelReason::ShuttingDown) {
        return StateSyncError::ShuttingDown;
      }
      return StateSyncError::Network;
    }

    template <typename Stream>
    void cancel_stream_socket(Stream &stream) {
      // Stream is either:
      // - beast::tcp_stream
      // - beast::ssl_stream<beast::tcp_stream>
      if constexpr (std::is_same_v<Stream, beast::tcp_stream>) {
        stream.socket().cancel();
      } else {
        stream.next_layer().socket().cancel();
      }
    }

    template <typename Stream>
    net::awaitable<void> shutdown_watcher(
        const std::atomic_flag &is_shutting_down,
        net::steady_timer &timer,
        Stream &stream,
        std::shared_ptr<CancelState> cancel_state) {
      for (;;) {
        if (is_shutting_down.test()) {
          cancel_state->set_if_none(CancelReason::ShuttingDown);
          cancel_stream_socket(stream);
          co_return;
        }

        timer.expires_after(100ms);

        beast::error_code ec;
        co_await timer.async_wait(net::redirect_error(net::use_awaitable, ec));
        if (ec == net::error::operation_aborted) {
          co_return;
        }
      }
    }

    template <typename Stream>
    net::awaitable<outcome::result<State>> do_http_flow(
        tcp::resolver &resolver,
        Stream &stream,
        const UrlParts &parts,
        const std::chrono::seconds timeout,
        const std::atomic_flag &is_shutting_down,
        const bool do_tls_handshake) {
      auto ex = co_await net::this_coro::executor;

      if (is_shutting_down.test()) [[unlikely]] {
        co_return StateSyncError::ShuttingDown;
      }

      beast::error_code ec;

      auto results = co_await resolver.async_resolve(
          parts.host, parts.port, net::redirect_error(net::use_awaitable, ec));
      if (ec) {
        co_return StateSyncError::Network;
      }

      auto cancel_state = std::make_shared<CancelState>();

      net::steady_timer timeout_timer(ex);
      timeout_timer.expires_after(timeout);

      net::co_spawn(
          ex,
          [&]() -> net::awaitable<void> {
            beast::error_code tec;
            co_await timeout_timer.async_wait(
                net::redirect_error(net::use_awaitable, tec));
            if (not tec) {
              cancel_state->set_if_none(CancelReason::Timeout);
              cancel_stream_socket(stream);
            }
            co_return;
          },
          net::detached);

      net::steady_timer shutdown_timer(ex);
      net::co_spawn(ex,
                    shutdown_watcher(
                        is_shutting_down, shutdown_timer, stream, cancel_state),
                    net::detached);

      if constexpr (std::is_same_v<Stream, beast::tcp_stream>) {
        stream.expires_after(timeout);
        co_await stream.async_connect(
            results, net::redirect_error(net::use_awaitable, ec));
      } else {
        stream.next_layer().expires_after(timeout);
        co_await stream.next_layer().async_connect(
            results, net::redirect_error(net::use_awaitable, ec));
      }

      if (ec) {
        if (ec == net::error::operation_aborted) {
          co_return map_aborted(*cancel_state);
        }
        co_return StateSyncError::Network;
      }

      if constexpr (not std::is_same_v<Stream, beast::tcp_stream>) {
        if (do_tls_handshake) {
          co_await stream.async_handshake(
              ssl::stream_base::client,
              net::redirect_error(net::use_awaitable, ec));
          if (ec) {
            if (ec == net::error::operation_aborted) {
              co_return map_aborted(*cancel_state);
            }
            co_return StateSyncError::Network;
          }
        }
      } else {
        (void)do_tls_handshake;
      }

      http::request<http::empty_body> req{http::verb::get, parts.target, 11};
      req.set(http::field::host, parts.host);
      req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
      req.set(http::field::accept, "application/octet-stream");

      co_await http::async_write(
          stream, req, net::redirect_error(net::use_awaitable, ec));
      if (ec) {
        if (ec == net::error::operation_aborted) {
          co_return map_aborted(*cancel_state);
        }
        co_return StateSyncError::Network;
      }

      beast::flat_buffer buffer;
      http::response<http::vector_body<std::uint8_t>> res;

      co_await http::async_read(
          stream, buffer, res, net::redirect_error(net::use_awaitable, ec));
      if (ec) {
        if (ec == net::error::operation_aborted) {
          co_return map_aborted(*cancel_state);
        }
        co_return StateSyncError::Network;
      }

      timeout_timer.cancel();
      shutdown_timer.cancel();

      if (res.result() != http::status::ok) {
        co_return StateSyncError::HttpBadStatus;
      }
      auto &encoded_state = res.body();

      auto state_res = decode<State>(encoded_state);
      if (state_res.has_error()) {
        co_return StateSyncError::DeserializeFailed;
      }
      auto st = state_res.value();

      if constexpr (not std::is_same_v<Stream, beast::tcp_stream>) {
        if (do_tls_handshake) {
          co_await stream.async_shutdown(
              net::redirect_error(net::use_awaitable, ec));
        }
      }

      co_return st;
    }

    net::awaitable<outcome::result<State>> fetch_http_async(
        const UrlParts &parts,
        const std::chrono::seconds timeout,
        const std::atomic_flag &is_shutting_down) {
      auto ex = co_await net::this_coro::executor;
      tcp::resolver resolver(ex);

      beast::tcp_stream stream(ex);

      co_return co_await do_http_flow(
          resolver, stream, parts, timeout, is_shutting_down, false);
    }

    net::awaitable<outcome::result<State>> fetch_https_async(
        ssl::context &ssl_ctx,
        const UrlParts &parts,
        const std::chrono::seconds timeout,
        const std::atomic_flag &is_shutting_down) {
      auto ex = co_await net::this_coro::executor;
      tcp::resolver resolver(ex);

      beast::ssl_stream<beast::tcp_stream> stream(ex, ssl_ctx);

      // Verify the peer certificate and ensure it matches the requested
      // hostname.
      stream.set_verify_mode(ssl::verify_peer);
      stream.set_verify_callback(
          boost::asio::ssl::host_name_verification(parts.host));

      // SNI is required by many hosts.
      if (not SSL_set_tlsext_host_name(stream.native_handle(),
                                       parts.host.c_str())) {
        co_return StateSyncError::Network;
      }

      co_return co_await do_http_flow(
          resolver, stream, parts, timeout, is_shutting_down, true);
    }

  }  // namespace

  StateSyncClient::StateSyncClient(
      qtils::SharedRef<AsioSslContext> ssl_ctx,
      qtils::SharedRef<app::StateManager> state_manager)
      : ssl_ctx_(std::move(ssl_ctx)), state_manager_(std::move(state_manager)) {
    state_manager_->takeControl(*this);
  }

  void StateSyncClient::stop() {
    is_shutting_down_.test_and_set();
  }

  outcome::result<State> StateSyncClient::fetch(const std::string &url,
                                                std::chrono::seconds timeout) {
    if (busy_.test_and_set(std::memory_order_acq_rel)) {
      return StateSyncError::Busy;
    }
    auto unset_busy_flag =
        qtils::FinalAction{[this] { busy_.clear(std::memory_order_release); }};

    if (is_shutting_down_.test()) {
      return StateSyncError::ShuttingDown;
    }

    OUTCOME_TRY(url_parts, parse_url(url));

    net::io_context io(1);

    auto prom = std::make_shared<std::promise<outcome::result<State>>>();
    auto fut = prom->get_future();

    auto run_task = [&](auto &&task) {
      using Task = std::decay_t<decltype(task)>;
      net::co_spawn(
          io,
          [t = Task(std::forward<decltype(task)>(task)),
           p = prom]() mutable -> net::awaitable<void> {
            try {
              p->set_value(co_await std::move(t));
            } catch (...) {
              p->set_value(StateSyncError::Network);
            }
            co_return;
          },
          net::detached);
    };

    if (url_parts.scheme == "https") {
      run_task(
          fetch_https_async(*ssl_ctx_, url_parts, timeout, is_shutting_down_));
    } else if (url_parts.scheme == "http") {
      run_task(fetch_http_async(url_parts, timeout, is_shutting_down_));
    } else {
      return StateSyncError::UnsupportedScheme;
    }

    io.run();

    try {
      return fut.get();
    } catch (const std::exception &) {
      return StateSyncError::Network;
    }
  }

}  // namespace lean
