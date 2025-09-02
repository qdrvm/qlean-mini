/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <schnorrkel.h>

#include <qtils/byte_arr.hpp>

namespace lean::crypto::ed25519 {
  using Secret = qtils::ByteArr<ED25519_SECRET_KEY_LENGTH>;
  using Public = qtils::ByteArr<ED25519_PUBLIC_KEY_LENGTH>;
  using KeyPair = qtils::ByteArr<ED25519_KEYPAIR_LENGTH>;
  using Signature = qtils::ByteArr<ED25519_SIGNATURE_LENGTH>;
  using Message = qtils::BytesIn;

  inline std::optional<Signature> sign(const KeyPair &keypair,
                                       Message message) {
    Signature sig;
    auto res = ed25519_sign(
        sig.data(), keypair.data(), message.data(), message.size_bytes());
    if (res != ED25519_RESULT_OK) {
      return std::nullopt;
    }
    return sig;
  }

  inline bool verify(const Signature &signature,
                     Message message,
                     const Public &public_key) {
    auto res = ed25519_verify(signature.data(),
                              public_key.data(),
                              message.data(),
                              message.size_bytes());
    return res == ED25519_RESULT_OK;
  }
}  // namespace lean::crypto::ed25519
