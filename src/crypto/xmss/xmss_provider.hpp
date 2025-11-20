/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "crypto/xmss/types.hpp"

namespace lean::crypto::xmss {

  class XmssProvider {
   public:
    virtual ~XmssProvider() = default;

    virtual XmssKeypair generateKeypair(uint64_t activation_epoch,
                                        uint64_t num_active_epochs) = 0;

    virtual XmssSignature sign(XmssPrivateKey xmss_private_key,
                               uint32_t epoch,
                               qtils::BytesIn message) = 0;

    virtual bool verify(XmssPublicKey xmss_public_key,
                        qtils::BytesIn message,
                        XmssSignature xmss_signature) = 0;
  };
}  // namespace lean::crypto::xmss
