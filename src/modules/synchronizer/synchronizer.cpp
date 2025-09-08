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
  }
}  // namespace lean::modules
