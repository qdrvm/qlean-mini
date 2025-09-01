/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */


#include "modules/networking/networking.hpp"


namespace lean::modules {

  NetworkingImpl::NetworkingImpl(
      NetworkingLoader &loader,
      qtils::SharedRef<log::LoggingSystem> logging_system)
      : loader_(loader),
        logger_(logging_system->getLogger("Networking", "networking_module")) {}

  void NetworkingImpl::on_loaded_success() {
    SL_INFO(logger_, "Loaded success");
  }

  void NetworkingImpl::on_loading_is_finished() {
    SL_INFO(logger_, "Loading is finished");

    // tmp entry point for experiments
    auto x = std::make_shared<const messages::BlockAnnounceMessage>();
    loader_.dispatch_block_announce(std::move(x));
  }

  void NetworkingImpl::on_block_request(
      std::shared_ptr<const messages::BlockRequestMessage> msg) {
    SL_INFO(logger_, "Block requested");

    // tmp entry point for experiments
    auto x = std::make_shared<const messages::BlockResponseMessage>(
        messages::BlockResponseMessage{.ctx = msg->ctx, .result = Block{}});
    loader_.dispatch_block_response(std::move(x));
  };

}  // namespace lean::modules
