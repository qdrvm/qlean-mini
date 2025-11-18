/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "crypto/xmss/xmss_provider.hpp"

namespace lean::crypto::keystore {

  class KeyStore {
   public:
    virtual ~KeyStore() = default;

    virtual xmss::XmssKeypair xmssKeypair() const = 0;
  };

}  // namespace lean::crypto::keystore
