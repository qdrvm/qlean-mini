/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/lists.hpp>

#include "types/constants.hpp"
#include "types/lean_aggregated_signature.hpp"

namespace lean {
  using AttestationSignatures =
      ssz::list<LeanAggregatedSignature, VALIDATOR_REGISTRY_LIMIT>;
}  // namespace lean
