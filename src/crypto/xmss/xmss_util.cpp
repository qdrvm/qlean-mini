/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "crypto/xmss/xmss_util.hpp"

#include <algorithm>
#include <fstream>
#include <memory>
#include <sstream>

#include <c_hash_sig/c_hash_sig.h>

OUTCOME_CPP_DEFINE_CATEGORY(lean::crypto::xmss, XmssUtilError, e) {
  using E = lean::crypto::xmss::XmssUtilError;
  switch (e) {
    case E::FileNotFound:
      return "Key file not found";
    case E::FileOpenFailed:
      return "Failed to open key file";
    case E::JsonParseFailed:
      return "Failed to parse JSON key";
    case E::SerializationFailed:
      return "Failed to serialize key";
  }
  return "Unknown XmssUtilError";
}

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
    outcome::result<std::string> loadFileAsString(
        const std::filesystem::path &path) {
      if (!std::filesystem::exists(path)) {
        return XmssUtilError::FileNotFound;
      }

      std::ifstream file(path);
      if (!file.is_open()) {
        return XmssUtilError::FileOpenFailed;
      }

      std::stringstream buffer;
      buffer << file.rdbuf();
      return buffer.str();
    }

    // Parse public key from JSON file
    outcome::result<XmssPublicKey> parsePublicKeyFromJson(
        const std::filesystem::path &path) {
      OUTCOME_TRY(json_content, loadFileAsString(path));

      PQSignatureSchemePublicKey *pk_raw = nullptr;
      PQSigningError result =
          pq_public_key_from_json(json_content.c_str(), &pk_raw);

      if (result != PQ_SIGNING_ERROR_SUCCESS) {
        return XmssUtilError::JsonParseFailed;
      }

      PublicKeyPtr pk(pk_raw);

      // Serialize to bytes
      constexpr size_t kMaxPublicKeySize = 100;
      qtils::ByteVec pk_buffer(kMaxPublicKeySize);
      size_t pk_written = 0;

      result = pq_public_key_serialize(
          pk.get(), pk_buffer.data(), pk_buffer.size(), &pk_written);

      if (result != PQ_SIGNING_ERROR_SUCCESS) {
        return XmssUtilError::SerializationFailed;
      }

      XmssPublicKey pk_bytes;
      if (pk_written != pk_bytes.size()) {
        return XmssUtilError::SerializationFailed;
      }
      std::copy_n(pk_buffer.begin(), pk_written, pk_bytes.begin());
      return pk_bytes;
    }

    // Parse secret key from JSON file
    outcome::result<XmssPrivateKey> parseSecretKeyFromJson(
        const std::filesystem::path &path) {
      OUTCOME_TRY(json_content, loadFileAsString(path));

      PQSignatureSchemeSecretKey *sk_raw = nullptr;
      PQSigningError result =
          pq_secret_key_from_json(json_content.c_str(), &sk_raw);

      if (result != PQ_SIGNING_ERROR_SUCCESS) {
        return XmssUtilError::JsonParseFailed;
      }

      SecretKeyPtr sk(sk_raw);

      return XmssPrivateKey{std::move(sk)};
    }
  }  // namespace

  outcome::result<XmssKeypair> loadKeypairFromJson(
      const std::filesystem::path &secret_key_path,
      const std::filesystem::path &public_key_path) {
    XmssKeypair keypair;

    OUTCOME_TRY(pk, parsePublicKeyFromJson(public_key_path));
    keypair.public_key = std::move(pk);

    OUTCOME_TRY(sk, parseSecretKeyFromJson(secret_key_path));
    keypair.private_key = std::move(sk);

    return keypair;
  }

}  // namespace lean::crypto::xmss
