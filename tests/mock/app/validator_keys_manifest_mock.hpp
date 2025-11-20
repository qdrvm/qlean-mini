/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <gmock/gmock.h>

#include "app/validator_keys_manifest.hpp"

namespace lean::app {
  class ValidatorKeysManifestMock : public ValidatorKeysManifest {
   public:
    MOCK_METHOD(std::optional<crypto::xmss::XmssPublicKey>,
                getXmssPubkey,
                (ValidatorIndex),
                (const, override));
  };
}  // namespace lean::app
