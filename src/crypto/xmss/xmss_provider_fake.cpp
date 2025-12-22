/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "crypto/xmss/xmss_provider_fake.hpp"

#include <random>

#include <qtils/bytes_std_hash.hpp>

#include "utils/tuple_hash.hpp"

namespace lean::crypto::xmss {
  XmssKeypair XmssProviderFake::generateKeypair(uint64_t, uint64_t) {
    abort();
  }

  XmssSignature XmssProviderFake::sign(XmssPrivateKey private_key,
                                       uint32_t epoch,
                                       qtils::BytesIn message) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto key_index = reinterpret_cast<size_t>(private_key.get());
    auto args = std::tuple{key_index, epoch, qtils::BytesStdHash{}(message)};
    XmssSignature signature;
    uint32_t seed = std::hash<decltype(args)>{}(args);
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
