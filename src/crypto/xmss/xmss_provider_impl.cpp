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

namespace lean::crypto::xmss {

  namespace {
    // RAII wrapper for PQSignatureSchemeSecretKey
    struct SecretKeyDeleter {
      void operator()(PQSignatureSchemeSecretKey *key) const {
        if (key) {
          pq_secret_key_free(key);
        }
      }
    };
    using SecretKeyPtr =
        std::unique_ptr<PQSignatureSchemeSecretKey, SecretKeyDeleter>;

    // RAII wrapper for PQSignatureSchemePublicKey
    struct PublicKeyDeleter {
      void operator()(PQSignatureSchemePublicKey *key) const {
        if (key) {
          pq_public_key_free(key);
        }
      }
    };
    using PublicKeyPtr =
        std::unique_ptr<PQSignatureSchemePublicKey, PublicKeyDeleter>;

    // RAII wrapper for PQSignature
    struct SignatureDeleter {
      void operator()(PQSignature *sig) const {
        if (sig) {
          pq_signature_free(sig);
        }
      }
    };
    using SignaturePtr = std::unique_ptr<PQSignature, SignatureDeleter>;

    // RAII wrapper for error description string
    struct StringDeleter {
      void operator()(char *str) const {
        if (str) {
          pq_string_free(str);
        }
      }
    };
    using CStringPtr = std::unique_ptr<char, StringDeleter>;

    std::string getErrorDescription(PQSigningError error) {
      CStringPtr error_str(pq_error_description(error));
      return error_str ? std::string(error_str.get()) : "Unknown error";
    }
  }  // namespace

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

    PQSignatureSchemePublicKey *pk_raw = nullptr;
    PQSignatureSchemeSecretKey *sk_raw = nullptr;

    PQSigningError result =
        pq_key_gen(activation_epoch, num_active_epochs, &pk_raw, &sk_raw);
    if (result != 0) {  // Success = 0
      throw std::runtime_error("Failed to generate XMSS keypair: "
                               + getErrorDescription(result));
    }

    PublicKeyPtr pk(pk_raw);
    SecretKeyPtr sk(sk_raw);

    // Serialize keys to byte vectors
    XmssKeypair keypair;

    keypair.private_key = std::move(sk);

    // Serialize public key
    constexpr size_t kMaxPublicKeySize = 100;
    qtils::ByteVec pk_buffer(kMaxPublicKeySize);
    size_t pk_written = 0;

    result = pq_public_key_serialize(
        pk.get(), pk_buffer.data(), pk_buffer.size(), &pk_written);
    if (result != 0) {  // Success = 0
      throw std::runtime_error("Failed to serialize XMSS public key: "
                               + getErrorDescription(result));
    }
    BOOST_ASSERT_MSG(pk_written == keypair.public_key.size(),
                     "Serialized XMSS public key size mismatch");
    std::copy_n(pk_buffer.begin(), pk_written, keypair.public_key.begin());

    return keypair;
  }

  XmssSignature XmssProviderImpl::sign(XmssPrivateKey xmss_private_key,
                                       uint32_t epoch,
                                       qtils::BytesIn message) {
    auto &sk = xmss_private_key;
    PQSigningError result{};

    // Sign the message
    PQSignature *signature_raw = nullptr;
    result = pq_sign(
        sk.get(), epoch, message.data(), message.size(), &signature_raw);

    if (result != 0) {  // Success = 0
      throw std::runtime_error("Failed to sign message with XMSS: "
                               + getErrorDescription(result));
    }
    SignaturePtr signature(signature_raw);

    // Serialize signature
    constexpr size_t kMaxSignatureSize = 10000;  // 10KB buffer
    qtils::ByteVec sig_buffer(kMaxSignatureSize);
    size_t sig_written = 0;

    result = pq_signature_serialize(
        signature.get(), sig_buffer.data(), sig_buffer.size(), &sig_written);
    if (result != 0) {  // Success = 0
      throw std::runtime_error("Failed to serialize XMSS signature: "
                               + getErrorDescription(result));
    }
    sig_buffer.resize(sig_written);

    XmssSignature final_signature;
    std::fill(final_signature.begin(), final_signature.end(), 0);

    if (sig_written > final_signature.size()) {
      throw std::runtime_error(
          "XMSS signature too large: " + std::to_string(sig_written)
          + " expected: " + std::to_string(final_signature.size()));
    }

    std::memcpy(final_signature.data(), sig_buffer.data(), sig_written);

    return final_signature;
  }

  bool XmssProviderImpl::verify(XmssPublicKey xmss_public_key,
                                qtils::BytesIn message,
                                uint32_t epoch,
                                XmssSignature xmss_signature) {
    // Deserialize public key
    PQSignatureSchemePublicKey *pk_raw = nullptr;
    PQSigningError result = pq_public_key_deserialize(
        xmss_public_key.data(), xmss_public_key.size(), &pk_raw);

    if (result != 0) {  // Success = 0
      throw std::runtime_error("Failed to deserialize XMSS public key: "
                               + getErrorDescription(result));
    }
    PublicKeyPtr pk(pk_raw);

    // Deserialize signature
    PQSignature *signature_raw = nullptr;
    result = pq_signature_deserialize(
        xmss_signature.data(), xmss_signature.size(), &signature_raw);

    if (result != 0) {  // Success = 0
      throw std::runtime_error("Failed to deserialize XMSS signature: "
                               + getErrorDescription(result));
    }
    SignaturePtr signature(signature_raw);

    // Verify signature
    int verify_result = pq_verify(
        pk.get(), epoch, message.data(), message.size(), signature.get());

    if (verify_result < 0) {
      throw std::runtime_error("Error during XMSS signature verification");
    }

    return verify_result == 1;
  }

}  // namespace lean::crypto::xmss
