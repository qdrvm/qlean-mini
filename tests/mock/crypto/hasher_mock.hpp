/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "crypto/hasher.hpp"
#include <gmock/gmock.h>
#include <qtils/byte_view.hpp>

namespace lean::crypto {

  class HasherMock : public Hasher {
   public:
    ~HasherMock() override = default;

    MOCK_METHOD(Hash64, twox_64, (qtils::ByteView), (const, override));

    MOCK_METHOD(Hash64, blake2b_64, (qtils::ByteView), (const, override));

    MOCK_METHOD(Hash128, blake2b_128, (qtils::ByteView), (const, override));

    MOCK_METHOD(Hash128, twox_128, (qtils::ByteView), (const, override));

    MOCK_METHOD(Hash256, twox_256, (qtils::ByteView), (const, override));

    MOCK_METHOD(Hash256, blake2b_256, (qtils::ByteView), (const, override));

    MOCK_METHOD(Hash256, blake2s_256, (qtils::ByteView), (const, override));

    MOCK_METHOD(Hash256, keccak_256, (qtils::ByteView), (const, override));

    MOCK_METHOD(Hash256, sha2_256, (qtils::ByteView), (const, override));

    MOCK_METHOD(Hash512, blake2b_512, (qtils::ByteView), (const, override));
  };

}  // namespace kagome::crypto
