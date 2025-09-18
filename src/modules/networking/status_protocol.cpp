/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "modules/networking/status_protocol.hpp"

#include <libp2p/basic/read_varint.hpp>
#include <libp2p/basic/write_varint.hpp>
#include <libp2p/coro/spawn.hpp>
#include <libp2p/host/basic_host.hpp>

#include "modules/networking/ssz_snappy.hpp"

namespace lean::modules {
  StatusProtocol::StatusProtocol(
      std::shared_ptr<boost::asio::io_context> io_context,
      std::shared_ptr<libp2p::host::BasicHost> host,
      GetStatus get_status,
      OnStatus on_status)
      : io_context_{std::move(io_context)},
        host_{std::move(host)},
        get_status_{std::move(get_status)},
        on_status_{std::move(on_status)} {}

  libp2p::StreamProtocols StatusProtocol::getProtocolIds() const {
    return {"/leanconsensus/req/status/1/ssz_snappy"};
  }

  void StatusProtocol::handle(std::shared_ptr<libp2p::Stream> stream) {
    libp2p::coroSpawn(
        *io_context_,
        [self{shared_from_this()}, stream]() -> libp2p::Coro<void> {
          std::ignore = co_await self->coroHandle(stream);
        });
  }

  void StatusProtocol::start() {
    host_->listenProtocol(shared_from_this());
  }

  libp2p::CoroOutcome<void> StatusProtocol::connect(
      std::shared_ptr<libp2p::connection::CapableConnection> connection) {
    BOOST_OUTCOME_CO_TRY(
        auto stream, co_await host_->newStream(connection, getProtocolIds()));
    BOOST_OUTCOME_CO_TRY(co_await coroHandle(stream));
    co_return outcome::success();
  }

  libp2p::CoroOutcome<void> StatusProtocol::coroHandle(
      std::shared_ptr<libp2p::Stream> stream) {
    auto peer_id = stream->remotePeerId();
    BOOST_OUTCOME_CO_TRY(co_await libp2p::writeVarintMessage(
        stream, encodeSszSnappy(get_status_())));
    qtils::ByteVec encoded;
    BOOST_OUTCOME_CO_TRY(co_await libp2p::readVarintMessage(stream, encoded));
    BOOST_OUTCOME_CO_TRY(auto status, decodeSszSnappy<StatusMessage>(encoded));
    on_status_(messages::StatusMessageReceived{
        .from_peer = peer_id,
        .notification = status,
    });
    co_return outcome::success();
  }
}  // namespace lean::modules
