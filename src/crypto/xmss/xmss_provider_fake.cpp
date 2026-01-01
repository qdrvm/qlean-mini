/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "crypto/xmss/xmss_provider_fake.hpp"

#include <random>

#include <qtils/bytestr.hpp>

#include "utils/tuple_hash.hpp"

namespace lean::crypto::xmss {
  constexpr size_t kAggregatedSignatureSize = 857289;

  void randomBytesSeed(qtils::BytesOut out, uint32_t seed) {
    std::independent_bits_engine<std::default_random_engine, 8, uint8_t> random(
        seed);
    std::ranges::generate(out, random);
  }

  XmssKeypair XmssProviderFake::generateKeypair(uint64_t, uint64_t) {
    abort();
  }

  XmssSignature XmssProviderFake::sign(XmssPrivateKey private_key,
                                       uint32_t epoch,
                                       const XmssMessage &message) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto key_index = reinterpret_cast<size_t>(private_key.get());
    size_t seed = 0;
    boost::hash_combine(seed, key_index);
    boost::hash_combine(seed, epoch);
    boost::hash_combine(seed, message);
    XmssSignature signature;
    randomBytesSeed(signature, seed);
    return signature;
  }

  bool XmssProviderFake::verify(const XmssPublicKey &,
                                const XmssMessage &,
                                uint32_t,
                                const XmssSignature &) {
    return true;
  }

  XmssAggregatedSignature XmssProviderFake::aggregateSignatures(
      std::span<const XmssPublicKey> public_keys,
      std::span<const XmssSignature> signatures,
      uint32_t epoch,
      const XmssMessage &message) const {
    size_t seed = 0;
    boost::hash_combine(seed, public_keys);
    boost::hash_combine(seed, signatures);
    boost::hash_combine(seed, epoch);
    boost::hash_combine(seed, message);
    XmssAggregatedSignature signature;
    signature.resize(kAggregatedSignatureSize);
    randomBytesSeed(signature, seed);
    return signature;
  }

  bool XmssProviderFake::verifyAggregatedSignatures(
      std::span<const XmssPublicKey>,
      uint32_t,
      const XmssMessage &,
      const XmssAggregatedSignature &) const {
    return true;
  }

  XmssKeypair XmssProviderFake::loadKeypair(std::string_view private_key_path) {
    size_t key_index = std::hash<std::string_view>{}(private_key_path);
    return XmssKeypair{
        .private_key =
            {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                reinterpret_cast<PQSecretKey *>(key_index),
                [](PQSecretKey *) {},
            },
    };
  }
}  // namespace lean::crypto::xmss
