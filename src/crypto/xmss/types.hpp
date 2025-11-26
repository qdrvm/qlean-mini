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
  using XmssPrivateKey = std::shared_ptr<PQSignatureSchemeSecretKey>;
  using XmssPublicKey = qtils::ByteArr<52>;
  struct XmssKeypair {
    XmssPrivateKey private_key;
    XmssPublicKey public_key;
  };
  using XmssSignature = qtils::ByteArr<3116>;
}  // namespace lean::crypto::xmss
