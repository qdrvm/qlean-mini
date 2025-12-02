/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include <c_hash_sig/c_hash_sig.h>
#include <qtils/enum_error_code.hpp>
#include <qtils/outcome.hpp>

Q_ENUM_ERROR_CODE(PQSigningError) {
  return pq_error_description(e);
}

namespace lean::crypto::xmss::ffi {
  inline outcome::result<void> asOutcome(PQSigningError error) {
    if (error == PQSigningError::PQ_SIGNING_ERROR_SUCCESS) {
      return outcome::success();
    }
    return make_error_code(error);
  }

  template <typename T, void (*f)(T *)>
  struct FfiPtrDeleter {
    static void operator()(T *ptr) {
      if (ptr != nullptr) {
        f(ptr);
      }
    }
  };

  template <typename T, void (*f)(T *)>
  using FfiPtr = std::unique_ptr<T, FfiPtrDeleter<T, f>>;

  using SecretKey = FfiPtr<PQSecretKey, pq_secret_key_free>;
  using PublicKey = FfiPtr<PQPublicKey, pq_public_key_free>;
  using Signature = FfiPtr<PQSignature, pq_signature_free>;
}  // namespace lean::crypto::xmss::ffi
