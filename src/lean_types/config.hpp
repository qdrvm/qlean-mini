/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

namespace lean {

  /// @note temporary property to support simplified round robin block
  /// production in absence of randao & deposit mechanisms
  struct Config {
    uint64_t num_validators;
    uint64_t genesis_time;
  };


}  // namespace lean
