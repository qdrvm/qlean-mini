/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "crypto/hash_types.hpp"
#include "crypto/hasher.hpp"

namespace lean::crypto {

  class HasherImpl : public Hasher {
   public:
    ~HasherImpl() override = default;

    Hash64 twox_64(qtils::ByteView data) const override;

    Hash64 blake2b_64(qtils::ByteView data) const override;

    Hash128 twox_128(qtils::ByteView data) const override;

    Hash128 blake2b_128(qtils::ByteView data) const override;

    Hash256 twox_256(qtils::ByteView data) const override;

    Hash256 blake2b_256(qtils::ByteView data) const override;

    Hash256 keccak_256(qtils::ByteView data) const override;

    Hash256 blake2s_256(qtils::ByteView data) const override;

    Hash256 sha2_256(qtils::ByteView data) const override;

    Hash512 blake2b_512(qtils::ByteView data) const override;
  };

}  // namespace lean::crypto
