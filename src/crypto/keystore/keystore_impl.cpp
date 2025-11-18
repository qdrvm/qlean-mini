/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "crypto/keystore/keystore_impl.hpp"

#include <c_hash_sig/c_hash_sig.h>

#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace lean::crypto::keystore {

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

    // Load entire file as string
    std::string loadFileAsString(const std::filesystem::path& path) {
      if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Key file not found: " + path.string());
      }

      std::ifstream file(path);
      if (!file.is_open()) {
        throw std::runtime_error("Failed to open key file: " + path.string());
      }

      std::stringstream buffer;
      buffer << file.rdbuf();
      return buffer.str();
    }

    // Parse public key from JSON file
    xmss::XmssPublicKey parsePublicKeyFromJson(const std::filesystem::path& path) {
      std::string json_content = loadFileAsString(path);

      PQSignatureSchemePublicKey *pk_raw = nullptr;
      PQSigningError result = pq_public_key_from_json(json_content.c_str(), &pk_raw);

      if (result != PQ_SIGNING_ERROR_SUCCESS) {
        throw std::runtime_error("Failed to parse public key from JSON: "
                               + getErrorDescription(result));
      }

      PublicKeyPtr pk(pk_raw);

      // Serialize to bytes
      constexpr size_t kMaxPublicKeySize = 100;
      xmss::XmssPublicKey pk_bytes(kMaxPublicKeySize);
      size_t pk_written = 0;

      result = pq_public_key_serialize(
          pk.get(), pk_bytes.data(), pk_bytes.size(), &pk_written);

      if (result != PQ_SIGNING_ERROR_SUCCESS) {
        throw std::runtime_error("Failed to serialize public key: "
                               + getErrorDescription(result));
      }

      pk_bytes.resize(pk_written);
      return pk_bytes;
    }

    // Parse secret key from JSON file
    xmss::XmssPrivateKey parseSecretKeyFromJson(const std::filesystem::path& path) {
      std::string json_content = loadFileAsString(path);

      PQSignatureSchemeSecretKey *sk_raw = nullptr;
      PQSigningError result = pq_secret_key_from_json(json_content.c_str(), &sk_raw);

      if (result != PQ_SIGNING_ERROR_SUCCESS) {
        throw std::runtime_error("Failed to parse secret key from JSON: "
                               + getErrorDescription(result));
      }

      SecretKeyPtr sk(sk_raw);

      // Serialize to bytes
      // Secret keys can be very large (50+ MB for large epoch counts)
      size_t max_secret_key_size = 100 * 1024 * 1024; // 100 MB buffer
      xmss::XmssPrivateKey sk_bytes(max_secret_key_size);
      size_t sk_written = 0;

      result = pq_secret_key_serialize(
          sk.get(), sk_bytes.data(), sk_bytes.size(), &sk_written);

      if (result != PQ_SIGNING_ERROR_SUCCESS) {
        throw std::runtime_error("Failed to serialize secret key: "
                               + getErrorDescription(result));
      }

      sk_bytes.resize(sk_written);
      return sk_bytes;
    }
  }

  KeyStoreImpl::KeyStoreImpl(std::filesystem::path secret_key_path,
                             std::filesystem::path public_key_path) {
    keypair_.public_key = parsePublicKeyFromJson(public_key_path);
    keypair_.private_key = parseSecretKeyFromJson(secret_key_path);
  }

  xmss::XmssKeypair KeyStoreImpl::xmssKeypair() const {
    return keypair_;
  }

}  // namespace lean::crypto::keystore

