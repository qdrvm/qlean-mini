/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/basic/read.hpp>
#include <libp2p/basic/write.hpp>
#include <qtils/byte_arr.hpp>

namespace lean {
  inline libp2p::CoroOutcome<void> writeResponseStatus(
      std::shared_ptr<libp2p::basic::Writer> writer) {
    qtils::ByteArr<1> status{0};
    BOOST_OUTCOME_CO_TRY(co_await libp2p::write(writer, status));
    co_return outcome::success();
  }

  inline libp2p::CoroOutcome<void> readResponseStatus(
      std::shared_ptr<libp2p::basic::Reader> reader) {
    qtils::ByteArr<1> status;
    BOOST_OUTCOME_CO_TRY(co_await libp2p::read(reader, status));
    co_return outcome::success();
  }
}  // namespace lean
