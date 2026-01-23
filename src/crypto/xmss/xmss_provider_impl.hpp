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
                       const XmssMessage &message) override;

    bool verify(const XmssPublicKey &xmss_public_key,
                const XmssMessage &message,
                uint32_t epoch,
                const XmssSignature &xmss_signature) override;

    XmssAggregatedSignature aggregateSignatures(
        std::span<const XmssPublicKey> public_keys,
        std::span<const XmssSignature> signatures,
        uint32_t epoch,
        const XmssMessage &message) const override;
    bool verifyAggregatedSignatures(
        std::span<const XmssPublicKey> public_keys,
        uint32_t epoch,
        const XmssMessage &message,
        XmssAggregatedSignatureIn aggregated_signature) const override;
  };

}  // namespace lean::crypto::xmss
