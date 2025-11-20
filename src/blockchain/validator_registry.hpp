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

    [[nodiscard]] virtual ValidatorIndices allValidatorsIndices() const = 0;

    [[nodiscard]] virtual std::optional<std::string> nodeIdByIndex(
        ValidatorIndex index) const = 0;

    [[nodiscard]] virtual std::optional<ValidatorIndices>
    validatorIndicesForNodeId(std::string_view node_id) const = 0;
  };
}  // namespace lean
