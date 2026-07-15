/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "crypto/xmss/xmss_provider_impl.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <ranges>
#include <stdexcept>

#include <c_hash_sig/c_hash_sig.h>

#include "crypto/xmss/ffi.hpp"
#include "metrics/metrics.hpp"

namespace lean::crypto::xmss {
  using const_u8_ptr = const uint8_t *;

  constexpr size_t LOG_INV_RATE_PROD = 2;

  qtils::ByteVec ffiByteVec(PQByteVec &&v) {
    qtils::ByteVec r{std::span{v.ptr, v.size}};
    PQByteVec_drop(v);
    return r;
  }

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
    std::vector<const_u8_ptr> items_raw;
    items_raw.reserve(items.size());
    for (auto &item : items) {
      items_raw.emplace_back(item.data());
    }
    return items_raw;
  }

  XmssAggregatedSignature XmssProviderImpl::aggregateSignatures(
      std::span<const std::vector<XmssPublicKey>> child_public_keys,
      std::span<const XmssAggregatedSignature> child_proofs,
      std::span<const XmssPublicKey> public_keys,
      std::span<const XmssSignature> signatures,
      uint32_t epoch,
      const XmssMessage &message) const {
    std::optional<metrics::HistogramTimer> timer{};
    if (use_metrics_) {
      timer.emplace(
          metrics_->lean_pq_sig_aggregated_signatures_building_time_seconds()
              ->timer());
    }

    if (child_public_keys.size() != child_proofs.size()) {
      throw std::logic_error{
          "XmssProviderImpl::aggregateSignatures child public key and proof "
          "count mismatch"};
    }
    if (public_keys.size() != signatures.size()) {
      throw std::logic_error{
          "XmssProviderImpl::aggregateSignatures public key and signature "
          "count mismatch"};
    }
    auto public_keys_raw = manyToRaw(public_keys);
    auto signatures_raw = manyToRaw(signatures);
    std::vector<std::vector<const_u8_ptr>> ffi_public_keys;
    ffi_public_keys.reserve(child_proofs.size());
    for (auto &keys : child_public_keys) {
      auto &ffi_keys = ffi_public_keys.emplace_back();
      ffi_keys.reserve(keys.size());
      for (auto &key : keys) {
        ffi_keys.emplace_back(key.data());
      }
    }
    std::vector<PQChildProof> ffi_children;
    ffi_children.reserve(child_proofs.size());
    for (auto &&[public_keys, proof] :
         std::views::zip(ffi_public_keys, child_proofs)) {
      ffi_children.emplace_back(PQChildProof{
          .proof_ptr = proof.data(),
          .proof_size = proof.size(),
          .public_keys_bytes_ptr = public_keys.data(),
          .public_keys_count = public_keys.size(),
      });
    }
    XmssAggregatedSignature aggregated_signature =
        ffiByteVec(pq_aggregate_signatures(ffi_children.data(),
                                           ffi_children.size(),
                                           public_keys.size(),
                                           public_keys_raw.data(),
                                           signatures_raw.data(),
                                           epoch,
                                           message.data(),
                                           LOG_INV_RATE_PROD));

    if (use_metrics_) {
      metrics_->pq_sig_attestations_in_aggregated_signatures_total()->inc(
          signatures.size()  // NOLINT(cppcoreguidelines-narrowing-conversions)
      );
      metrics_->pq_sig_aggregated_signatures_total()->inc();
    }

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
        metrics_->lean_pq_sig_aggregated_signatures_valid_total()->inc();
      } else {
        metrics_->lean_pq_sig_aggregated_signatures_invalid_total()->inc();
      }
    }

    return is_valid;
  }

  struct PublicKeyVecVec {
    PublicKeyVecVec(const std::vector<std::vector<XmssPublicKey>> &vec_vec) {
      ptr.reserve(vec_vec.size());
      ptr_ptr.reserve(vec_vec.size());
      ptr_size.reserve(vec_vec.size());
      for (auto &vec : vec_vec) {
        auto &row = ptr.emplace_back();
        row.reserve(vec.size());
        for (auto &key : vec) {
          row.emplace_back(key.data());
        }
      }
      for (auto &row : ptr) {
        ptr_ptr.emplace_back(row.data());
        ptr_size.emplace_back(row.size());
      }
    }
    std::vector<std::vector<const_u8_ptr>> ptr;
    std::vector<const const_u8_ptr *> ptr_ptr;
    std::vector<size_t> ptr_size;
  };

  TypeTwoMultiSignature XmssProviderImpl::aggregateTypeTwo(
      const std::vector<std::vector<XmssPublicKey>> &public_keys,
      const std::vector<XmssAggregatedSignature> &type_one_signatures) const {
    PublicKeyVecVec keys{public_keys};
    assert(public_keys.size() == type_one_signatures.size());
    auto count = public_keys.size();
    std::vector<const_u8_ptr> type_1_ptr;
    std::vector<size_t> type_1_size;
    type_1_ptr.reserve(count);
    type_1_size.reserve(count);
    for (auto &type_1 : type_one_signatures) {
      type_1_ptr.emplace_back(type_1.data());
      type_1_size.emplace_back(type_1.size());
    }
    return TypeTwoMultiSignature{
        .proof = ffiByteVec(pq_aggregate_type_two(count,
                                                  keys.ptr_ptr.data(),
                                                  keys.ptr_size.data(),
                                                  type_1_ptr.data(),
                                                  type_1_size.data(),
                                                  LOG_INV_RATE_PROD)),
    };
  }

  bool XmssProviderImpl::verifyTypeTwo(
      const std::vector<std::vector<XmssPublicKey>> &public_keys,
      const TypeTwoMultiSignature &type_two_signature,
      EpochsAndMessages epochs_and_messages) const {
    PublicKeyVecVec keys{public_keys};
    assert(public_keys.size() == epochs_and_messages.size());
    auto count = public_keys.size();
    std::vector<uint32_t> epochs;
    std::vector<const_u8_ptr> messages;
    epochs.reserve(count);
    messages.reserve(count);
    for (auto &[epoch, message] : epochs_and_messages) {
      epochs.emplace_back(epoch);
      messages.emplace_back(message.data());
    }
    return pq_verify_type_two(type_two_signature.proof.data().data(),
                              type_two_signature.proof.size(),
                              public_keys.size(),
                              keys.ptr_ptr.data(),
                              keys.ptr_size.data(),
                              epochs.data(),
                              messages.data());
  }

  XmssAggregatedSignature XmssProviderImpl::splitTypeTwo(
      const std::vector<std::vector<XmssPublicKey>> &public_keys,
      const TypeTwoMultiSignature &type_two_signature,
      size_t index) const {
    PublicKeyVecVec keys{public_keys};
    return ffiByteVec(pq_split_type_two(type_two_signature.proof.data().data(),
                                        type_two_signature.proof.size(),
                                        public_keys.size(),
                                        keys.ptr_ptr.data(),
                                        keys.ptr_size.data(),
                                        index,
                                        LOG_INV_RATE_PROD));
  }
}  // namespace lean::crypto::xmss
