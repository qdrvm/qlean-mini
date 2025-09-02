/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <log/logger.hpp>
#include <modules/networking/interfaces.hpp>
#include <qtils/create_smart_pointer_macros.hpp>
#include <qtils/shared_ref.hpp>
#include <utils/ctor_limiters.hpp>

namespace lean::modules {

  class NetworkingImpl final : public Singleton<Networking>, public Networking {
   public:
    static std::shared_ptr<Networking> instance;
    CREATE_SHARED_METHOD(NetworkingImpl);

    NetworkingImpl(NetworkingLoader &loader,
                   qtils::SharedRef<log::LoggingSystem> logging_system);

    void on_loaded_success() override;

    void on_loading_is_finished() override;

    void on_block_request(
        std::shared_ptr<const messages::BlockRequestMessage> msg) override;

   private:
    NetworkingLoader &loader_;
    log::Logger logger_;
  };

}  // namespace lean::modules
