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
    crypto::xmss::XmssPublicKey attestation_pubkey;
    crypto::xmss::XmssPublicKey proposal_pubkey;
    ValidatorIndex index;

    SSZ_AND_JSON_FIELDS(attestation_pubkey, proposal_pubkey, index);
    bool operator==(const Validator &) const = default;
  };
}  // namespace lean
