/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <gmock/gmock.h>

#include "crypto/xmss/xmss_provider.hpp"

namespace lean::crypto::xmss {
  class XmssProviderMock : public XmssProvider {
   public:
    MOCK_METHOD(XmssKeypair, generateKeypair, (uint64_t, uint64_t), (override));
    MOCK_METHOD(XmssSignature,
                sign,
                (XmssPrivateKey, uint32_t, const XmssMessage &),
                (override));
    MOCK_METHOD(bool,
                verify,
                (const XmssPublicKey &,
                 const XmssMessage &,
                 uint32_t,
                 const XmssSignature &),
                (override));
    MOCK_METHOD(XmssAggregatedSignature,
                aggregateSignatures,
                (std::span<const XmssPublicKey>,
                 std::span<const XmssSignature>,
                 uint32_t,
                 const XmssMessage &),
                (const, override));
    MOCK_METHOD(bool,
                verifyAggregatedSignatures,
                (std::span<const XmssPublicKey>,
                 uint32_t,
                 const XmssMessage &,
                 const XmssAggregatedSignature &),
                (const, override));
  };
}  // namespace lean::crypto::xmss
