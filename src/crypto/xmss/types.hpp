/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <c_hash_sig/c_hash_sig.h>
#include <qtils/bytes.hpp>

namespace lean::crypto::xmss {
  using XmssPrivateKey = qtils::ByteVec;
  using XmssPublicKey = qtils::ByteArr<52>;
  struct XmssKeypair {
    XmssPrivateKey private_key;
    XmssPublicKey public_key;
  };
  using XmssSignature = qtils::ByteVec;
}  // namespace lean::crypto::xmss
