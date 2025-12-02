/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "crypto/xmss/xmss_provider_fake.hpp"

#include <random>

#include <qtils/bytes_std_hash.hpp>

namespace lean::crypto::xmss {
  XmssKeypair XmssProviderFake::generateKeypair(uint64_t, uint64_t) {
    abort();
  }

  XmssSignature XmssProviderFake::sign(XmssPrivateKey,
                                       uint32_t epoch,
                                       qtils::BytesIn message) {
    XmssSignature signature;
    uint32_t seed = epoch ^ qtils::BytesStdHash{}(message);
    std::independent_bits_engine<std::default_random_engine, 8, uint8_t> random(
        seed);
    std::ranges::generate(signature, random);
    return signature;
  }

  bool XmssProviderFake::verify(XmssPublicKey,
                                qtils::BytesIn,
                                uint32_t,
                                XmssSignature) {
    return true;
  }
}  // namespace lean::crypto::xmss
