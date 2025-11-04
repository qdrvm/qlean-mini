/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/byte_arr.hpp>
#include <sszpp/container.hpp>

namespace lean {
  struct Validator : ssz::ssz_container {
    qtils::ByteArr<52> pubkey;

    SSZ_CONT(pubkey);
    bool operator==(const Validator &) const = default;
  };
}  // namespace lean
