/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/shared_ref.hpp>

#include "crypto/xmss/xmss_provider.hpp"

namespace lean::metrics {
  class Metrics;
}

namespace lean::crypto::xmss {

  class XmssProviderImpl : public XmssProvider {
   public:
    XmssProviderImpl() = default;

    explicit XmssProviderImpl(qtils::SharedRef<metrics::Metrics> metrics);

    ~XmssProviderImpl() override = default;

    XmssKeypair generateKeypair(uint64_t activation_epoch,
                                uint64_t num_active_epochs) override;

    XmssSignature sign(XmssPrivateKey xmss_private_key,
                       uint32_t epoch,
                       const XmssMessage &message) override;

    bool verify(const XmssPublicKey &xmss_public_key,
                const XmssMessage &message,
                uint32_t epoch,
                const XmssSignature &xmss_signature) override;

    [[nodiscard]] XmssAggregatedSignature aggregateSignatures(
        std::span<const XmssPublicKey> public_keys,
        std::span<const XmssSignature> signatures,
        uint32_t epoch,
        const XmssMessage &message) const override;
    [[nodiscard]] bool verifyAggregatedSignatures(
        std::span<const XmssPublicKey> public_keys,
        uint32_t epoch,
        const XmssMessage &message,
        XmssAggregatedSignatureIn aggregated_signature) const override;

   private:
    bool use_metrics_ = false;
    std::shared_ptr<metrics::Metrics> metrics_;
  };

}  // namespace lean::crypto::xmss
