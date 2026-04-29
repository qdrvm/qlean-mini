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
    MOCK_METHOD(std::optional<crypto::xmss::XmssKeypair>,
                getKeypair,
                (const crypto::xmss::XmssPublicKey &),
                (const, override));

    MOCK_METHOD(std::vector<crypto::xmss::XmssPublicKey>,
                getAllXmssPubkeys,
                (),
                (const, override));
  };
}  // namespace lean::app
