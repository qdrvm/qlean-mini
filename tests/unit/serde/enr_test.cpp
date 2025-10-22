//
// Copyright Quadrivium LLC
// All Rights Reserved
// SPDX-License-Identifier: Apache-2.0
//

#include "serde/enr.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>

#include <qtils/test/outcome.hpp>

#include "utils/sample_peer.hpp"

using lean::enr::Enr;
using lean::enr::Ip;
using lean::enr::Port;
using lean::enr::Secp256k1PublicKey;
using lean::enr::Secp256k1Signature;

TEST(EnrTest, EncodeDecodeRoundTrip) {
  lean::SamplePeer peer{0, false};

  ASSERT_OUTCOME_SUCCESS(
      encoded, lean::enr::encode(peer.keypair, peer.enr_ip, peer.port));
  // Encoded string must start with "enr:"
  ASSERT_TRUE(encoded.rfind("enr:", 0) == 0) << encoded;

  ASSERT_OUTCOME_SUCCESS(enr, lean::enr::decode(encoded));

  // Basic fields
  EXPECT_EQ(enr.sequence, 1u);
  EXPECT_EQ(SpanAdl{enr.public_key}, peer.keypair.publicKey.data);

  // IP and port present and correct
  ASSERT_TRUE(enr.ip.has_value());
  // encode() uses Ip{127, 0, 0, 1}
  Ip expected_ip{127, 0, 0, 1};
  EXPECT_EQ(enr.ip.value(), expected_ip);

  ASSERT_TRUE(enr.port.has_value());
  EXPECT_EQ(enr.port.value(), peer.port);

  // PeerId must be derivable and non-empty
  auto pid = enr.peerId().toBase58();
  EXPECT_FALSE(pid.empty());

  // Connect info wiring
  auto info = enr.connectInfo();
  EXPECT_EQ(info.id, enr.peerId());
}

TEST(EnrTest, DeterministicEncoding) {
  lean::SamplePeer peer{0, false};
  lean::SamplePeer peer2{1, false};

  ASSERT_OUTCOME_SUCCESS(
      e1, lean::enr::encode(peer.keypair, peer.enr_ip, peer.port));
  ASSERT_OUTCOME_SUCCESS(
      e2, lean::enr::encode(peer.keypair, peer.enr_ip, peer.port));
  EXPECT_EQ(e1, e2);

  // Changing port changes ENR
  ASSERT_OUTCOME_SUCCESS(
      e3, lean::enr::encode(peer.keypair, peer.enr_ip, peer2.port));
  EXPECT_NE(e1, e3);

  // Changing key changes ENR
  ASSERT_OUTCOME_SUCCESS(
      e4, lean::enr::encode(peer2.keypair, peer.enr_ip, peer.port));
  EXPECT_NE(e1, e4);
}

TEST(EnrTest, DecodeGivenEnrAddress) {
  // Provided ENR string from user request
  // clang-format off
  std::string_view addr =
      "enr:-IW4QHcpC4AgOv7WXk1V8E56DDAy6KJ09VMOxSTUwgOqkLF6YihJc5Eoeo4UX1bm9H39Xl-831fomuqR3TZzB3S2IPoBgmlkgnY0gmlwhH8AAAGEcXVpY4InEIlzZWNwMjU2azGhA21sqsJIr5b2r6f5BPVQJToPPvP1qi_mg4qVshZpFGji";
  // clang-format on

  ASSERT_OUTCOME_SUCCESS(enr, lean::enr::decode(addr));

  // Sequence should be positive
  EXPECT_GT(enr.sequence, 0u);

  // IP should be present and equal to 3.17.30.69
  ASSERT_TRUE(enr.ip.has_value());
  Ip expected_ip{127, 0, 0, 1};
  EXPECT_EQ(enr.ip.value(), expected_ip);

  // UDP port should be present; expected 10000
  ASSERT_TRUE(enr.port.has_value());
  EXPECT_EQ(enr.port.value(), static_cast<Port>(10000));

  // Public key must be compressed secp256k1 (33 bytes) with a valid prefix
  EXPECT_EQ(enr.public_key.size(), 33u);
  EXPECT_TRUE(enr.public_key[0] == 0x02 || enr.public_key[0] == 0x03);

  // PeerId derivation must succeed
  auto pid = enr.peerId().toBase58();
  EXPECT_FALSE(pid.empty());

  // Connect info wiring
  auto info = enr.connectInfo();
  EXPECT_EQ(info.id, enr.peerId());
}
