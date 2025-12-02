/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#include "types/attestation.hpp"
#include "types/signature.hpp"

namespace lean {
  struct SignedAttestation : ssz::ssz_container {
    Attestation message;
    Signature signature;

    SSZ_CONT(message, signature);
  };
}  // namespace lean
