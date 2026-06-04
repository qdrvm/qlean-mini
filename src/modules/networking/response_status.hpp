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
  constexpr uint8_t kResponseStatusSuccess = 0;
  constexpr uint8_t kResponseStatusInvalidRequest = 1;

  inline libp2p::CoroOutcome<void> writeResponseStatus(
      std::shared_ptr<libp2p::basic::Writer> writer, uint8_t status) {
    qtils::ByteArr<1> status_bytes{status};
    BOOST_OUTCOME_CO_TRY(co_await libp2p::write(writer, status_bytes));
    co_return outcome::success();
  }

  inline libp2p::CoroOutcome<uint8_t> readResponseStatus(
      std::shared_ptr<libp2p::basic::Reader> reader) {
    qtils::ByteArr<1> status;
    BOOST_OUTCOME_CO_TRY(co_await libp2p::read(reader, status));
    co_return status[0];
  }
}  // namespace lean
