/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/common/sample_peer.hpp>

#include "serde/enr.hpp"

namespace lean {
  struct SamplePeer : libp2p::SamplePeer {
    SamplePeer(size_t index) : libp2p::SamplePeer{makeSecp256k1(index)} {}

    std::string enr() const {
      enr::Secp256k1PublicKey public_key;
      assert(keypair.publicKey.data.size() == public_key.size());
      memcpy(
          public_key.data(), keypair.publicKey.data.data(), public_key.size());
      return enr::encode(public_key, port);
    }
  };
}  // namespace lean
