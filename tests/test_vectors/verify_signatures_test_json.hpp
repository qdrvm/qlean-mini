/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <optional>

#include "serde/json_fwd.hpp"
#include "types/signed_block.hpp"
#include "types/state.hpp"

namespace lean {
  struct VerifySignaturesTestJson {
    State anchor_state;
    SignedBlock signed_block;
    std::optional<std::string> rejection_reason;

    JSON_FIELDS(anchor_state, signed_block, rejection_reason);
  };
}  // namespace lean
