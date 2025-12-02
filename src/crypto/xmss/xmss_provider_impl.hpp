/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "crypto/xmss/xmss_provider.hpp"

namespace lean::crypto::xmss {

  class XmssProviderImpl : public XmssProvider {
   public:
    XmssKeypair generateKeypair(uint64_t activation_epoch,
                                uint64_t num_active_epochs) override;

    XmssSignature sign(XmssPrivateKey xmss_private_key,
                       uint32_t epoch,
                       qtils::BytesIn message) override;

    bool verify(XmssPublicKey xmss_public_key,
                qtils::BytesIn message,
                uint32_t epoch,
                XmssSignature xmss_signature) override;
  };

}  // namespace lean::crypto::xmss
