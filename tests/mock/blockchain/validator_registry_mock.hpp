/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <gmock/gmock.h>

#include "blockchain/validator_registry.hpp"

namespace lean {
  class ValidatorRegistryMock : public ValidatorRegistry {
   public:
    MOCK_METHOD(const ValidatorIndices &,
                currentValidatorIndices,
                (),
                (const, override));
    MOCK_METHOD(ValidatorIndices, allValidatorsIndices, (), (const, override));
    MOCK_METHOD(std::optional<std::string>,
                nodeIdByIndex,
                (ValidatorIndex),
                (const, override));
    MOCK_METHOD(std::optional<ValidatorIndices>,
                validatorIndicesForNodeId,
                (std::string_view),
                (const, override));
  };
}  // namespace lean
