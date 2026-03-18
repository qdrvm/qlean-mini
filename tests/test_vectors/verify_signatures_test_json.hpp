/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <optional>

#include "serde/json_fwd.hpp"
#include "types/signed_block_with_attestation.hpp"
#include "types/state.hpp"

namespace lean {
  struct VerifySignaturesTestJson {
    State anchor_state;
    SignedBlockWithAttestation signed_block_with_attestation;
    std::optional<std::string> expect_exception;

    JSON_FIELDS(anchor_state, signed_block_with_attestation, expect_exception);
  };
}  // namespace lean
