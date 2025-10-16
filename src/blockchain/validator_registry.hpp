/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <unordered_set>

#include "types/validator_index.hpp"

namespace lean {
  class ValidatorRegistry {
   public:
    using ValidatorIndices = std::unordered_set<ValidatorIndex>;

    virtual ~ValidatorRegistry() = default;

    [[nodiscard]] virtual const ValidatorIndices &currentValidatorIndices()
        const = 0;
  };
}  // namespace lean
