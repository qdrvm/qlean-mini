/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <optional>

#include <libp2p/multi/multiaddress.hpp>
#include <libp2p/peer/peer_info.hpp>
#include <qtils/bytes.hpp>

namespace lean::enr {
  using Secp256k1Signature = qtils::ByteArr<64>;
  using Sequence = uint64_t;
  using Secp256k1PublicKey = qtils::ByteArr<33>;
  using Ip = qtils::ByteArr<4>;
  using Port = uint16_t;

  struct Enr {
    Secp256k1Signature signature;
    Sequence sequence;
    Secp256k1PublicKey public_key;
    std::optional<Ip> ip;
    std::optional<Port> port;

    libp2p::PeerId peerId() const;
    libp2p::Multiaddress listenAddress() const;
    libp2p::Multiaddress connectAddress() const;
    libp2p::PeerInfo connectInfo() const;
  };

  Enr decode(std::string_view str);

  std::string encode(const Secp256k1PublicKey &public_key, Port port);
}  // namespace lean::enr
