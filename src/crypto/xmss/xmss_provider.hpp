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
                               const XmssMessage &message) = 0;

    virtual bool verify(const XmssPublicKey &xmss_public_key,
                        const XmssMessage &message,
                        uint32_t epoch,
                        const XmssSignature &xmss_signature) = 0;

    virtual XmssAggregatedSignature aggregateSignatures(
        std::span<const XmssPublicKey> public_keys,
        std::span<const XmssSignature> signatures,
        uint32_t epoch,
        const XmssMessage &message) const = 0;

    virtual bool verifyAggregatedSignatures(
        std::span<const XmssPublicKey> public_keys,
        uint32_t epoch,
        const XmssMessage &message,
        XmssAggregatedSignatureIn aggregated_signature) const = 0;
  };
}  // namespace lean::crypto::xmss
