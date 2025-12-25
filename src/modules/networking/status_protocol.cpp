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

#include "modules/networking/response_status.hpp"
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
    BOOST_OUTCOME_CO_TRY(co_await write(stream));
    BOOST_OUTCOME_CO_TRY(co_await readResponseStatus(stream));
    BOOST_OUTCOME_CO_TRY(co_await read(stream));
    co_return outcome::success();
  }

  libp2p::CoroOutcome<void> StatusProtocol::read(
      std::shared_ptr<libp2p::Stream> stream) {
    auto peer_id = stream->remotePeerId();
    qtils::ByteVec encoded;
    BOOST_OUTCOME_CO_TRY(co_await libp2p::readVarintMessage(stream, encoded));
    BOOST_OUTCOME_CO_TRY(auto status,
                         decodeSszSnappyFramed<StatusMessage>(encoded));
    on_status_(messages::StatusMessageReceived{
        .from_peer = peer_id,
        .notification = status,
    });
    co_return outcome::success();
  }

  libp2p::CoroOutcome<void> StatusProtocol::write(
      std::shared_ptr<libp2p::Stream> stream) {
    BOOST_OUTCOME_CO_TRY(co_await libp2p::writeVarintMessage(
        stream, encodeSszSnappyFramed(get_status_())));
    co_return outcome::success();
  }

  libp2p::CoroOutcome<void> StatusProtocol::coroHandle(
      std::shared_ptr<libp2p::Stream> stream) {
    BOOST_OUTCOME_CO_TRY(co_await read(stream));
    BOOST_OUTCOME_CO_TRY(co_await writeResponseStatus(stream));
    BOOST_OUTCOME_CO_TRY(co_await write(stream));
    co_return outcome::success();
  }
}  // namespace lean::modules
