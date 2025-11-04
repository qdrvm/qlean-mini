/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "types/attestation_data.hpp"
#include "types/validator_index.hpp"

namespace lean {
  struct Attestation : ssz::ssz_container {
    ValidatorIndex validator_id;
    AttestationData data;

    SSZ_CONT(validator_id, data);
    bool operator==(const Attestation &) const = default;
  };
}  // namespace lean
