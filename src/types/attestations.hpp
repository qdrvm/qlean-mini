/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/lists.hpp>

#include "types/attestation.hpp"
#include "types/constants.hpp"

namespace lean {
  using Attestations = ssz::list<Attestation, VALIDATOR_REGISTRY_LIMIT>;
}  // namespace lean
