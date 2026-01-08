/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "crypto/xmss/xmss_provider.hpp"

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
    // XmssProvider
    XmssKeypair generateKeypair(uint64_t, uint64_t) override;
    XmssSignature sign(XmssPrivateKey,
                       uint32_t epoch,
                       qtils::BytesIn message) override;
    bool verify(XmssPublicKey,
                qtils::BytesIn,
                uint32_t,
                XmssSignature) override;

    static XmssKeypair loadKeypair(std::string_view private_key_path);
  };
}  // namespace lean::crypto::xmss
