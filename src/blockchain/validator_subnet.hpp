/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types/config.hpp"
#include "types/validator_index.hpp"

namespace lean {
  /**
   * Return subnet index for validator index.
   * Current spec uses round robin assignment.
   */
  inline SubnetIndex validatorSubnet(ValidatorIndex validator_index,
                                     uint64_t subnet_count) {
    return validator_index % subnet_count;
  }
}  // namespace lean
