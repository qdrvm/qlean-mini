/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "crypto/xmss/xmss_provider_impl.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>

#include <c_hash_sig/c_hash_sig.h>

#include "crypto/xmss/ffi.hpp"

namespace lean::crypto::xmss {
  XmssKeypair XmssProviderImpl::generateKeypair(uint64_t activation_epoch,
                                                uint64_t num_active_epochs) {
    // Validate parameters
    uint64_t max_lifetime = pq_get_lifetime();
    if (num_active_epochs == 0) {
      throw std::runtime_error(
          "Number of active epochs must be greater than 0");
    }
    if (num_active_epochs > max_lifetime) {
      throw std::runtime_error("Number of active epochs ("
                               + std::to_string(num_active_epochs)
                               + ") exceeds maximum lifetime ("
                               + std::to_string(max_lifetime) + ")");
    }

    PQPublicKey *public_key_raw = nullptr;
    PQSecretKey *secret_key_raw = nullptr;

    ffi::asOutcome(pq_key_gen(activation_epoch,
                              num_active_epochs,
                              &public_key_raw,
                              &secret_key_raw))
        .value();

    ffi::PublicKey public_key{public_key_raw};
    ffi::SecretKey secret_key{secret_key_raw};

    // Serialize keys to byte vectors
    XmssKeypair keypair;

    keypair.private_key = std::move(secret_key);

    // Serialize public key
    pq_public_key_to_bytes(public_key.get(), keypair.public_key.data());

    return keypair;
  }

  XmssSignature XmssProviderImpl::sign(XmssPrivateKey xmss_private_key,
                                       uint32_t epoch,
                                       qtils::BytesIn message) {
    // Sign the message
    PQSignature *signature_raw = nullptr;
    ffi::asOutcome(
        pq_sign(xmss_private_key.get(), epoch, message.data(), &signature_raw))
        .value();

    ffi::Signature signature{signature_raw};

    // Serialize signature
    XmssSignature signature_bytes;
    pq_signature_to_bytes(signature.get(), signature_bytes.data());
    return signature_bytes;
  }

  bool XmssProviderImpl::verify(XmssPublicKey xmss_public_key,
                                qtils::BytesIn message,
                                uint32_t epoch,
                                XmssSignature xmss_signature) {
    // Deserialize public key
    PQPublicKey *public_key_raw = nullptr;
    ffi::asOutcome(
        pq_public_key_from_bytes(xmss_public_key.data(), &public_key_raw))
        .value();
    ffi::PublicKey public_key{public_key_raw};

    // Deserialize signature
    PQSignature *signature_raw = nullptr;
    ffi::asOutcome(
        pq_signature_from_bytes(xmss_signature.data(), &signature_raw))
        .value();
    ffi::Signature signature{signature_raw};

    // Verify signature
    int verify_result =
        pq_verify(public_key.get(), epoch, message.data(), signature.get());

    if (verify_result < 0) {
      throw std::runtime_error("Error during XMSS signature verification");
    }

    return verify_result == 1;
  }

}  // namespace lean::crypto::xmss
