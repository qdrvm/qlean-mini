/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include <libp2p/protocol/base_protocol.hpp>
#include <qtils/shared_ref.hpp>

#include "modules/networking/types.hpp"

namespace boost::asio {
  class io_context;
}  // namespace boost::asio

namespace libp2p::host {
  class BasicHost;
}  // namespace libp2p::host

namespace lean {
  class ForkChoiceStoreMutex;
}  // namespace lean

namespace lean::blockchain {
  class BlockTree;
}  // namespace lean::blockchain

namespace lean::modules {
  class BlockByRangeProtocol
      : public std::enable_shared_from_this<BlockByRangeProtocol>,
        public libp2p::protocol::BaseProtocol {
   public:
    BlockByRangeProtocol(
        std::shared_ptr<boost::asio::io_context> io_context,
        std::shared_ptr<libp2p::host::BasicHost> host,
        qtils::SharedRef<blockchain::BlockTree> block_tree,
        qtils::SharedRef<ForkChoiceStoreMutex> fork_choice_store);

    // BaseProtocol
    libp2p::StreamProtocols getProtocolIds() const override;
    void handle(std::shared_ptr<libp2p::Stream> stream) override;

    void start();

    libp2p::CoroOutcome<BlocksResponse> request(libp2p::PeerId peer_id,
                                                BlocksByRangeRequest request);

   private:
    libp2p::CoroOutcome<void> coroRespond(
        std::shared_ptr<libp2p::Stream> stream);

    std::shared_ptr<boost::asio::io_context> io_context_;
    std::shared_ptr<libp2p::host::BasicHost> host_;
    qtils::SharedRef<blockchain::BlockTree> block_tree_;
    qtils::SharedRef<ForkChoiceStoreMutex> fork_choice_store_;
  };
}  // namespace lean::modules
