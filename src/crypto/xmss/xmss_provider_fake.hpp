/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/shared_ref.hpp>

#include "crypto/xmss/xmss_provider.hpp"

namespace lean::app {
  class Configuration;
}  // namespace lean::app

namespace lean::crypto::xmss {
  /**
   * Fake xmss provider implementation.
   * Used for shadow simulations.
   * `sign` returns pseudo-random `signature` seeded by `epoch` and `message`,
   * to prevent snappy from efficiently compressing zeros.
   * `verify` always return true.
   */
  class XmssProviderFake : public XmssProvider {
   public:
    XmssProviderFake(qtils::SharedRef<app::Configuration> app_config);

    // XmssProvider
    XmssKeypair generateKeypair(uint64_t, uint64_t) override;
    XmssSignature sign(XmssPrivateKey,
                       uint32_t epoch,
                       const XmssMessage &message) override;
    bool verify(const XmssPublicKey &,
                const XmssMessage &,
                uint32_t,
                const XmssSignature &) override;
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

    static XmssKeypair loadKeypair(std::string_view private_key_path);

   private:
    qtils::SharedRef<app::Configuration> app_config_;
  };
}  // namespace lean::crypto::xmss
