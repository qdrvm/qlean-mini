/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/config.hpp"
#include "types/validator_index.hpp"

namespace lean {
  inline bool validatorSubnet(ValidatorIndex validator_index,
                              const Config &config) {
    return validator_index % config.subnet_count;
  }
}  // namespace lean
