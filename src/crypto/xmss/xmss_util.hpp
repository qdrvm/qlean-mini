/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>

#include <qtils/enum_error_code.hpp>
#include <qtils/outcome.hpp>

#include "crypto/xmss/xmss_provider.hpp"

namespace lean::crypto::xmss {

  enum class XmssUtilError : uint8_t {
    FileNotFound,
    FileOpenFailed,
    JsonParseFailed,
    SerializationFailed,
  };

  /**
   * Load XMSS keypair from JSON files
   * @param secret_key_path Path to secret key JSON file (validator_X_sk.json)
   * @param public_key_path Path to public key JSON file (validator_X_pk.json)
   * @return Loaded XMSS keypair or error
   */
  outcome::result<XmssKeypair> loadKeypairFromJson(
      const std::filesystem::path &secret_key_path,
      const std::filesystem::path &public_key_path);

  std::string toJson(const XmssPrivateKey &sk);
  std::string toJson(const XmssPublicKey &pk_bytes);
}  // namespace lean::crypto::xmss

OUTCOME_HPP_DECLARE_ERROR(lean::crypto::xmss, XmssUtilError);
