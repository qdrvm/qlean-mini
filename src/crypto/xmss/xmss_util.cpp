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
#include <qtils/bytestr.hpp>

#include "crypto/xmss/ffi.hpp"
#include "qtils/read_file.hpp"

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
    // Parse public key from JSON file
    outcome::result<XmssPublicKey> parsePublicKeyFromJson(
        const std::filesystem::path &path) {
      BOOST_OUTCOME_TRY(auto json, qtils::readBytes(path));

      PQPublicKey *public_key_raw = nullptr;
      BOOST_OUTCOME_TRY(ffi::asOutcome(
          pq_public_key_from_json(json.data(), json.size(), &public_key_raw)));

      ffi::PublicKey public_key{public_key_raw};

      // Serialize to bytes
      XmssPublicKey public_key_bytes;
      pq_public_key_to_bytes(public_key.get(), public_key_bytes.data());
      return public_key_bytes;
    }

    // Parse secret key from JSON file
    outcome::result<XmssPrivateKey> parseSecretKeyFromJson(
        const std::filesystem::path &path) {
      BOOST_OUTCOME_TRY(auto json, qtils::readBytes(path));

      PQSecretKey *secret_key_raw = nullptr;
      BOOST_OUTCOME_TRY(ffi::asOutcome(
          pq_secret_key_from_json(json.data(), json.size(), &secret_key_raw)));

      ffi::SecretKey secret_key{secret_key_raw};

      return XmssPrivateKey{std::move(secret_key)};
    }
  }  // namespace

  outcome::result<XmssKeypair> loadKeypairFromJson(
      const std::filesystem::path &secret_key_path,
      const std::filesystem::path &public_key_path) {
    XmssKeypair keypair;

    BOOST_OUTCOME_TRY(auto public_key, parsePublicKeyFromJson(public_key_path));
    keypair.public_key = std::move(public_key);

    BOOST_OUTCOME_TRY(auto secret_key, parseSecretKeyFromJson(secret_key_path));
    keypair.private_key = std::move(secret_key);

    return keypair;
  }

  std::string toJson(const XmssPrivateKey &secret_key) {
    auto ffi_bytevec = pq_secret_key_to_json(secret_key.get());
    std::string json{qtils::byte2str({ffi_bytevec.ptr, ffi_bytevec.size})};
    PQByteVec_drop(ffi_bytevec);
    return json;
  }

  std::string toJson(const XmssPublicKey &public_key_bytes) {
    PQPublicKey *public_key_raw = nullptr;
    ffi::asOutcome(
        pq_public_key_from_bytes(public_key_bytes.data(), &public_key_raw))
        .value();
    ffi::PublicKey public_key{public_key_raw};
    auto ffi_bytevec = pq_public_key_to_json(public_key.get());
    std::string json{qtils::byte2str({ffi_bytevec.ptr, ffi_bytevec.size})};
    PQByteVec_drop(ffi_bytevec);
    return json;
  }
}  // namespace lean::crypto::xmss
