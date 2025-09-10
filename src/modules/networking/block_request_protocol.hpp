/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

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

namespace lean::blockchain {
  class BlockTree;
}  // namespace lean::blockchain

namespace lean::modules {
  class BlockRequestProtocol
      : public std::enable_shared_from_this<BlockRequestProtocol>,
        public libp2p::protocol::BaseProtocol {
   public:
    BlockRequestProtocol(std::shared_ptr<boost::asio::io_context> io_context,
                         std::shared_ptr<libp2p::host::BasicHost> host,
                         qtils::SharedRef<blockchain::BlockTree> block_tree);

    // BaseProtocol
    libp2p::StreamProtocols getProtocolIds() const override;
    void handle(std::shared_ptr<libp2p::Stream> stream) override;

    void start();

    libp2p::CoroOutcome<BlockResponse> request(libp2p::PeerId peer_id,
                                               BlockRequest request);

   private:
    libp2p::CoroOutcome<void> coroRespond(
        std::shared_ptr<libp2p::Stream> stream);

    std::shared_ptr<boost::asio::io_context> io_context_;
    std::shared_ptr<libp2p::host::BasicHost> host_;
    qtils::SharedRef<blockchain::BlockTree> block_tree_;
  };
}  // namespace lean::modules
