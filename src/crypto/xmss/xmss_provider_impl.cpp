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
#include "metrics/metrics.hpp"

namespace lean::crypto::xmss {

  XmssProviderImpl::XmssProviderImpl(qtils::SharedRef<metrics::Metrics> metrics)
      : use_metrics_(true), metrics_(std::move(metrics)) {}

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
                                       const XmssMessage &message) {
    std::optional<metrics::HistogramTimer> timer{};
    if (use_metrics_) {
      timer.emplace(metrics_->pq_sig_attestation_signing_time()->timer());
    }

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

  bool XmssProviderImpl::verify(const XmssPublicKey &xmss_public_key,
                                const XmssMessage &message,
                                uint32_t epoch,
                                const XmssSignature &xmss_signature) {
    std::optional<metrics::HistogramTimer> timer{};
    if (use_metrics_) {
      timer.emplace(metrics_->pq_sig_attestation_verification_time()->timer());
    }

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

  auto manyToRaw(const auto &items) {
    std::vector<const uint8_t *> items_raw;
    items_raw.reserve(items.size());
    for (auto &item : items) {
      items_raw.emplace_back(item.data());
    }
    return items_raw;
  }

  XmssAggregatedSignature XmssProviderImpl::aggregateSignatures(
      std::span<const XmssPublicKey> public_keys,
      std::span<const XmssSignature> signatures,
      uint32_t epoch,
      const XmssMessage &message) const {
    std::optional<metrics::HistogramTimer> timer{};
    if (use_metrics_) {
      timer.emplace(
          metrics_->pq_sig_attestation_signatures_building_time()->timer());
    }

    if (public_keys.size() != signatures.size()) {
      throw std::logic_error{
          "XmssProviderImpl::aggregateSignatures public key and signature "
          "count mismatch"};
    }
    auto public_keys_raw = manyToRaw(public_keys);
    auto signatures_raw = manyToRaw(signatures);
    auto ffi_bytevec = pq_aggregate_signatures(public_keys.size(),
                                               public_keys_raw.data(),
                                               signatures_raw.data(),
                                               epoch,
                                               message.data());
    XmssAggregatedSignature aggregated_signature{std::span{
        ffi_bytevec.ptr,
        ffi_bytevec.size,
    }};
    PQByteVec_drop(ffi_bytevec);
    return aggregated_signature;
  }

  bool XmssProviderImpl::verifyAggregatedSignatures(
      std::span<const XmssPublicKey> public_keys,
      uint32_t epoch,
      const XmssMessage &message,
      XmssAggregatedSignatureIn aggregated_signature) const {
    std::optional<metrics::HistogramTimer> timer{};
    if (use_metrics_) {
      timer.emplace(
          metrics_->pq_sig_aggregated_signatures_verification_time()->timer());
    }

    auto public_keys_raw = manyToRaw(public_keys);
    bool is_valid =
        pq_verify_aggregated_signatures(public_keys.size(),
                                        public_keys_raw.data(),
                                        epoch,
                                        message.data(),
                                        aggregated_signature.data(),
                                        aggregated_signature.size());
    if (use_metrics_) {
      if (is_valid) {
        metrics_->pq_sig_aggregated_signatures_valid_total()->inc();
      } else {
        metrics_->pq_sig_aggregated_signatures_invalid_total()->inc();
      }
    }

    return is_valid;
  }

}  // namespace lean::crypto::xmss
