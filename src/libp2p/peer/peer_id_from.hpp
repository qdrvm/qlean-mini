/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/crypto/key_marshaller.hpp>
#include <libp2p/crypto/secp256k1_types.hpp>
#include <libp2p/peer/peer_id.hpp>
#include <qtils/bytes.hpp>

namespace libp2p {
  /**
   * Convert secp256k1 public key to `PeerId`.
   */
  inline PeerId peerIdFromSecp256k1(
      const crypto::secp256k1::PublicKey &public_key) {
    return libp2p::PeerId::fromPublicKey(
               libp2p::crypto::marshaller::KeyMarshaller{nullptr}
                   .marshal(libp2p::crypto::PublicKey{
                       libp2p::crypto::Key::Type::Secp256k1,
                       qtils::ByteVec(public_key),
                   })
                   .value())
        .value();
  }
}  // namespace libp2p
