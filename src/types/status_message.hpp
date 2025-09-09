/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/checkpoint.hpp"

namespace lean {
  // https://github.com/leanEthereum/leanSpec/blob/main/docs/client/networking.md#status-v1
  struct StatusMessage : ssz::ssz_container {
    Checkpoint finalized;
    Checkpoint head;

    SSZ_CONT(finalized, head);
  };
}  // namespace lean
