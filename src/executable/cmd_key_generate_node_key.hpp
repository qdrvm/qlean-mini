/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <print>

#include <fmt/format.h>
#include <libp2p/crypto/random_generator/boost_generator.hpp>
#include <libp2p/crypto/secp256k1_provider/secp256k1_provider_impl.hpp>
#include <libp2p/peer/peer_id_from.hpp>

inline void cmdKeyGenerateNodeKey() {
  libp2p::crypto::secp256k1::Secp256k1ProviderImpl secp256k1{
      std::make_shared<libp2p::crypto::random::BoostRandomGenerator>()};
  auto keypair = secp256k1.generate().value();
  auto peer_id = libp2p::peerIdFromSecp256k1(keypair.public_key);
  std::println("{}", fmt::format("{:0xx}", qtils::Hex{keypair.private_key}));
  std::println("{}", peer_id.toBase58());
}
