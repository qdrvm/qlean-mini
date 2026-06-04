/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "modules/networking/response_status.hpp"
#include "modules/networking/ssz_snappy.hpp"

namespace lean {
  inline libp2p::CoroOutcome<void> writeResponseError(
      std::shared_ptr<libp2p::Stream> stream,
      uint8_t status,
      std::string_view error) {
    BOOST_OUTCOME_CO_TRY(
        co_await writeResponseStatus(stream, kResponseStatusInvalidRequest));
    ssz::list<uint8_t, 256> ssz_error;
    ssz_error.data() = qtils::ByteVec{qtils::str2byte(
        error.substr(0, std::min(error.size(), ssz_error.limit())))};
    BOOST_OUTCOME_CO_TRY(
        co_await snappy::coCompressFramed(stream, encode(ssz_error).value()));
    std::ignore = stream->close();
    co_return outcome::success();
  }
}  // namespace lean
