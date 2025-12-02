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
    [[nodiscard]] std::optional<crypto::xmss::XmssPublicKey>
    getXmssPubkeyByIndex(ValidatorIndex index) const override;

    [[nodiscard]] crypto::xmss::XmssKeypair currentNodeXmssKeypair()
        const override;

    [[nodiscard]] std::vector<crypto::xmss::XmssPublicKey> getAllXmssPubkeys()
        const override;

   private:
    std::unordered_map<ValidatorIndex, crypto::xmss::XmssPublicKey>
        validator_keys_;
    crypto::xmss::XmssKeypair current_node_xmss_keypair_;
  };
}  // namespace lean::app
