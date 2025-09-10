/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>

#include <libp2p/protocol/base_protocol.hpp>

#include "modules/shared/networking_types.tmp.hpp"

namespace boost::asio {
  class io_context;
}  // namespace boost::asio

namespace libp2p::host {
  class BasicHost;
}  // namespace libp2p::host

namespace lean::modules {
  class StatusProtocol : public std::enable_shared_from_this<StatusProtocol>,
                         public libp2p::protocol::BaseProtocol {
   public:
    using GetStatus = std::function<StatusMessage()>;
    using OnStatus = std::function<void(messages::StatusMessageReceived)>;

    StatusProtocol(std::shared_ptr<boost::asio::io_context> io_context,
                   std::shared_ptr<libp2p::host::BasicHost> host,
                   GetStatus get_status,
                   OnStatus on_status);

    // BaseProtocol
    libp2p::StreamProtocols getProtocolIds() const override;
    void handle(std::shared_ptr<libp2p::Stream> stream) override;

    void start();

    libp2p::CoroOutcome<void> connect(
        std::shared_ptr<libp2p::connection::CapableConnection> connection);

   private:
    libp2p::CoroOutcome<void> coroHandle(
        std::shared_ptr<libp2p::Stream> stream);

    std::shared_ptr<boost::asio::io_context> io_context_;
    std::shared_ptr<libp2p::host::BasicHost> host_;
    GetStatus get_status_;
    OnStatus on_status_;
  };
}  // namespace lean::modules
