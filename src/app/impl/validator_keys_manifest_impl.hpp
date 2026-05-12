/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "app/configuration.hpp"
#include "app/validator_keys_manifest.hpp"

namespace lean::app {

  class ValidatorKeysManifestImpl : public ValidatorKeysManifest {
   public:
    explicit ValidatorKeysManifestImpl(const Configuration &config);

    [[nodiscard]] std::optional<crypto::xmss::XmssKeypair> getKeypair(
        const crypto::xmss::XmssPublicKey &public_key) const override;

    [[nodiscard]] std::vector<crypto::xmss::XmssPublicKey> getAllXmssPubkeys()
        const override;

   private:
    std::unordered_map<crypto::xmss::XmssPublicKey, crypto::xmss::XmssKeypair>
        validator_keys_;
  };
}  // namespace lean::app
