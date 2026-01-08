/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "serde/json_fwd.hpp"
#include "types/attestation.hpp"
#include "types/block.hpp"

namespace lean {
  struct BlockWithAttestation : ssz::ssz_variable_size_container {
    Block block;
    Attestation proposer_attestation;

    SSZ_CONT(block, proposer_attestation);
    bool operator==(const BlockWithAttestation &) const = default;

    JSON_CAMEL(block, proposer_attestation);
  };
}  // namespace lean
