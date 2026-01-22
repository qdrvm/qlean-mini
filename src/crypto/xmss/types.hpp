/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include <c_hash_sig/c_hash_sig.h>
#include <qtils/bytes.hpp>

namespace lean::crypto::xmss {
  using XmssPrivateKey = std::shared_ptr<PQSecretKey>;
  using XmssPublicKey = qtils::ByteArr<PQ_PUBLIC_KEY_SIZE>;
  struct XmssKeypair {
    XmssPrivateKey private_key;
    XmssPublicKey public_key;
  };
  using XmssMessage = qtils::ByteArr<PQ_MESSAGE_SIZE>;
  using XmssSignature = qtils::ByteArr<PQ_SIGNATURE_SIZE>;
  using XmssAggregatedSignature = qtils::ByteVec;
  using XmssAggregatedSignatureIn = qtils::ByteView;
}  // namespace lean::crypto::xmss
