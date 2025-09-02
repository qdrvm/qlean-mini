/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */


#include "modules/synchronizer/synchronizer.hpp"

#include "modules/shared/networking_types.tmp.hpp"
#include "modules/shared/synchronizer_types.tmp.hpp"


namespace lean::modules {

  SynchronizerImpl::SynchronizerImpl(
      SynchronizerLoader &loader,
      qtils::SharedRef<log::LoggingSystem> logging_system)
      : loader_(loader),
        logger_(
            logging_system->getLogger("Synchronizer", "synchronizer_module")) {}

  void SynchronizerImpl::on_loaded_success() {
    SL_INFO(logger_, "Loaded success");
  }

  void SynchronizerImpl::on_block_index_discovered(
      std::shared_ptr<const messages::BlockDiscoveredMessage> msg) {
    SL_INFO(logger_, "Block discovered");
  };

  void SynchronizerImpl::on_block_announce(
      std::shared_ptr<const messages::BlockAnnounceMessage> msg) {
    SL_INFO(logger_, "Block announced");

    // tmp
    static const size_t s = reinterpret_cast<size_t>(this);
    static size_t n = 0;
    auto x = std::make_shared<const messages::BlockRequestMessage>(
        messages::BlockRequestMessage{.ctx = {{s, ++n}}});

    // block_response_callbacks_.emplace(x->ctx.rid, [&](auto& msg) {
    //   SL_INFO(logger_, "Block response has been handled; rid={}",
    //   msg->ctx.rid);
    // });
    loader_.dispatch_block_request(std::move(x));
  };

  void SynchronizerImpl::on_block_response(
      std::shared_ptr<const messages::BlockResponseMessage> msg) {
    auto it = block_response_callbacks_.find(msg->ctx.rid);
    if (it == block_response_callbacks_.end()) {
      SL_TRACE(logger_, "Received a response to someone else's request");
      return;
    }

    SL_INFO(logger_, "Block response is received; rid={}", msg->ctx.rid);
    // it->second(msg);
  }

}  // namespace lean::modules
