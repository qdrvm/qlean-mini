/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "crypto/xmss/types.hpp"
#include "types/type_two_multi_signature.hpp"

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

    [[nodiscard]] virtual XmssAggregatedSignature aggregateSignatures(
        std::span<const std::vector<XmssPublicKey>> child_public_keys,
        std::span<const XmssAggregatedSignature> child_proofs,
        std::span<const XmssPublicKey> public_keys,
        std::span<const XmssSignature> signatures,
        uint32_t epoch,
        const XmssMessage &message) const = 0;

    [[nodiscard]] virtual bool verifyAggregatedSignatures(
        std::span<const XmssPublicKey> public_keys,
        uint32_t epoch,
        const XmssMessage &message,
        XmssAggregatedSignatureIn aggregated_signature) const = 0;

    virtual TypeTwoMultiSignature aggregateTypeTwo(
        const std::vector<std::vector<XmssPublicKey>> &public_keys,
        const std::vector<XmssAggregatedSignature> &type_one_signatures)
        const = 0;

    using EpochsAndMessages = std::vector<std::pair<uint32_t, XmssMessage>>;
    virtual bool verifyTypeTwo(
        const std::vector<std::vector<XmssPublicKey>> &public_keys,
        const TypeTwoMultiSignature &type_two_signature,
        EpochsAndMessages epochs_and_messages) const = 0;

    virtual XmssAggregatedSignature splitTypeTwo(
        const std::vector<std::vector<XmssPublicKey>> &public_keys,
        const TypeTwoMultiSignature &type_two_signature,
        size_t index) const = 0;
  };
}  // namespace lean::crypto::xmss
