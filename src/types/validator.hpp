/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/byte_arr.hpp>
#include <sszpp/container.hpp>

#include "crypto/xmss/types.hpp"
#include "serde/json_fwd.hpp"
#include "types/validator_index.hpp"

namespace lean {
  struct Validator : ssz::ssz_container {
    crypto::xmss::XmssPublicKey attestation_public_key;
    crypto::xmss::XmssPublicKey proposal_public_key;
    ValidatorIndex index;

    SSZ_AND_JSON_FIELDS(attestation_public_key, proposal_public_key, index);
    bool operator==(const Validator &) const = default;
  };
}  // namespace lean
