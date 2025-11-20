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
    MOCK_METHOD(XmssKeypair,
                generateKeypair,
                (uint64_t, uint64_t),
                (override));
    MOCK_METHOD(XmssSignature,
                sign,
                (XmssPrivateKey, uint32_t, qtils::BytesIn),
                (override));
    MOCK_METHOD(bool,
                verify,
                (XmssPublicKey, qtils::BytesIn, XmssSignature),
                (override));
  };
}  // namespace lean::crypto::xmss
