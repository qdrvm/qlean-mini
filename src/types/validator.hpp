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
    crypto::xmss::XmssPublicKey pubkey;
    ValidatorIndex index;

    SSZ_CONT(pubkey, index);
    bool operator==(const Validator &) const = default;

    JSON_CAMEL(pubkey, index);
  };
}  // namespace lean
