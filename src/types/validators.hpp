/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/lists.hpp>

#include "types/constants.hpp"
#include "types/validator.hpp"

namespace lean {
  using Validators = ssz::list<Validator, VALIDATOR_REGISTRY_LIMIT>;
}  // namespace lean
