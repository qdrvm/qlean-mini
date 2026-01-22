/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/lists.hpp>

#include "types/aggregated_signature_proof.hpp"
#include "types/constants.hpp"

namespace lean {
  using AttestationSignatures =
      ssz::list<AggregatedSignatureProof, VALIDATOR_REGISTRY_LIMIT>;
}  // namespace lean
