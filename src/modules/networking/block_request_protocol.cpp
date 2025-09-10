/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "modules/networking/block_request_protocol.hpp"

#include <libp2p/basic/read_varint.hpp>
#include <libp2p/basic/write_varint.hpp>
#include <libp2p/coro/spawn.hpp>
#include <libp2p/host/basic_host.hpp>

#include "blockchain/block_tree.hpp"
#include "modules/networking/ssz_snappy.hpp"

namespace lean::modules {
  BlockRequestProtocol::BlockRequestProtocol(
      std::shared_ptr<boost::asio::io_context> io_context,
      std::shared_ptr<libp2p::host::BasicHost> host,
      qtils::SharedRef<blockchain::BlockTree> block_tree)
      : io_context_{std::move(io_context)},
        host_{std::move(host)},
        block_tree_{std::move(block_tree)} {}

  libp2p::StreamProtocols BlockRequestProtocol::getProtocolIds() const {
    return {"/leanconsensus/req/blocks_by_root/1/ssz_snappy"};
  }

  void BlockRequestProtocol::handle(std::shared_ptr<libp2p::Stream> stream) {
    libp2p::coroSpawn(
        *io_context_,
        [self{shared_from_this()}, stream]() -> libp2p::Coro<void> {
          std::ignore = co_await self->coroRespond(stream);
        });
  }

  void BlockRequestProtocol::start() {
    host_->listenProtocol(shared_from_this());
  }

  libp2p::CoroOutcome<BlockResponse> BlockRequestProtocol::request(
      libp2p::PeerId peer_id, BlockRequest request) {
    BOOST_OUTCOME_CO_TRY(auto stream,
                         co_await host_->newStream(peer_id, getProtocolIds()));
    BOOST_OUTCOME_CO_TRY(
        co_await libp2p::writeVarintMessage(stream, encodeSszSnappy(request)));
    qtils::ByteVec encoded;
    BOOST_OUTCOME_CO_TRY(co_await libp2p::readVarintMessage(stream, encoded));
    BOOST_OUTCOME_CO_TRY(auto response,
                         decodeSszSnappy<BlockResponse>(encoded));
    co_return response;
  }

  libp2p::CoroOutcome<void> BlockRequestProtocol::coroRespond(
      std::shared_ptr<libp2p::Stream> stream) {
    qtils::ByteVec encoded;
    BOOST_OUTCOME_CO_TRY(co_await libp2p::readVarintMessage(stream, encoded));
    BOOST_OUTCOME_CO_TRY(auto request, decodeSszSnappy<BlockRequest>(encoded));
    BlockResponse response;
    for (auto &block_hash : request.blocks) {
      BOOST_OUTCOME_CO_TRY(auto block,
                           block_tree_->tryGetSignedBlock(block_hash));
      if (block.has_value()) {
        response.blocks.push_back(std::move(block.value()));
      }
    }
    BOOST_OUTCOME_CO_TRY(
        co_await libp2p::writeVarintMessage(stream, encodeSszSnappy(response)));
    co_return outcome::success();
  }
}  // namespace lean::modules
