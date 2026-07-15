/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "modules/networking/block_by_range_protocol.hpp"

#include <libp2p/basic/read_varint.hpp>
#include <libp2p/basic/write_varint.hpp>
#include <libp2p/coro/spawn.hpp>
#include <libp2p/host/basic_host.hpp>

#include "blockchain/block_tree.hpp"
#include "blockchain/fork_choice_mutex.hpp"
#include "modules/networking/response_error.hpp"
#include "modules/networking/response_status.hpp"
#include "modules/networking/ssz_snappy.hpp"
#include "utils/saturating.hpp"

namespace lean::modules {
  BlockByRangeProtocol::BlockByRangeProtocol(
      std::shared_ptr<boost::asio::io_context> io_context,
      std::shared_ptr<libp2p::host::BasicHost> host,
      qtils::SharedRef<blockchain::BlockTree> block_tree,
      qtils::SharedRef<ForkChoiceStoreMutex> fork_choice_store)
      : io_context_{std::move(io_context)},
        host_{std::move(host)},
        block_tree_{std::move(block_tree)},
        fork_choice_store_{std::move(fork_choice_store)} {}

  libp2p::StreamProtocols BlockByRangeProtocol::getProtocolIds() const {
    return {"/leanconsensus/req/blocks_by_range/1/ssz_snappy"};
  }

  void BlockByRangeProtocol::handle(std::shared_ptr<libp2p::Stream> stream) {
    libp2p::coroSpawn(
        *io_context_,
        [self{shared_from_this()}, stream]() -> libp2p::Coro<void> {
          std::ignore = co_await self->coroRespond(stream);
        });
  }

  void BlockByRangeProtocol::start() {
    host_->listenProtocol(shared_from_this());
  }

  libp2p::CoroOutcome<BlocksResponse> BlockByRangeProtocol::request(
      libp2p::PeerId peer_id, BlocksByRangeRequest request) {
    BOOST_OUTCOME_CO_TRY(auto stream,
                         co_await host_->newStream(peer_id, getProtocolIds()));
    BOOST_OUTCOME_CO_TRY(
        co_await snappy::coCompressFramed(stream, encode(request).value()));
    std::ignore = stream->close();
    BlocksResponse response;
    while (true) {
      auto result_res = co_await readResponseStatus(stream);
      if (not result_res.has_value()) {
        break;
      }
      auto &result = result_res.value();
      if (result != kResponseStatusSuccess) {
        continue;
      }
      BOOST_OUTCOME_CO_TRY(auto encoded,
                           co_await snappy::coUncompressFramed(stream));
      BOOST_OUTCOME_CO_TRY(auto block, decode<SignedBlock>(encoded));
      response.emplace_back(std::move(block));
    }
    co_return response;
  }

  libp2p::CoroOutcome<void> BlockByRangeProtocol::coroRespond(
      std::shared_ptr<libp2p::Stream> stream) {
    BOOST_OUTCOME_CO_TRY(auto encoded,
                         co_await snappy::coUncompressFramed(stream));
    BOOST_OUTCOME_CO_TRY(auto request, decode<BlocksByRangeRequest>(encoded));
    if (request.count <= 0 or request.count > kMaxRequestBlocks) {
      BOOST_OUTCOME_CO_TRY(
          co_await writeResponseError(stream,
                                      kResponseStatusInvalidRequest,
                                      "invalid BlocksByRange request"));
      co_return outcome::success();
    }
    auto max_slot =
        saturatingSub(saturatingAdd(request.start_slot, request.count), 1);
    BlocksResponse response;
    auto hash = fork_choice_store_->getHead().root;
    while (true) {
      BOOST_OUTCOME_CO_TRY(auto header, block_tree_->tryGetBlockHeader(hash));
      if (not header.has_value()) {
        break;
      }
      if (header->slot < request.start_slot) {
        break;
      }
      if (header->slot <= max_slot) {
        BOOST_OUTCOME_CO_TRY(auto block, block_tree_->tryGetSignedBlock(hash));
        if (block.has_value()) {
          response.emplace_back(std::move(*block));
        }
      }
      hash = header->parent_root;
    }
    std::ranges::reverse(response);
    for (auto &block : response) {
      BOOST_OUTCOME_CO_TRY(
          co_await writeResponseStatus(stream, kResponseStatusSuccess));
      BOOST_OUTCOME_CO_TRY(
          co_await snappy::coCompressFramed(stream, encode(block).value()));
    }
    co_return outcome::success();
  }
}  // namespace lean::modules
