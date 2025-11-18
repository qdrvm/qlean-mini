/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "crypto/keystore/keystore.hpp"

#include <filesystem>

namespace lean::crypto::keystore {
  class KeyStoreImpl : public KeyStore {
   public:
    /**
     * Create KeyStore from JSON key files
     * @param secret_key_path Path to secret key JSON file (validator_X_sk.json)
     * @param public_key_path Path to public key JSON file (validator_X_pk.json)
     */
    KeyStoreImpl(std::filesystem::path secret_key_path,
                 std::filesystem::path public_key_path);

    xmss::XmssKeypair xmssKeypair() const override;

   private:
    xmss::XmssKeypair keypair_;
  };
}  // namespace lean::crypto::keystore
