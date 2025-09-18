//
// Copyright Quadrivium LLC
// All Rights Reserved
// SPDX-License-Identifier: Apache-2.0
//

#include "serde/enr.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>

using lean::enr::Enr;
using lean::enr::Ip;
using lean::enr::Port;
using lean::enr::Secp256k1PublicKey;
using lean::enr::Secp256k1Signature;

namespace {

  // Known valid compressed secp256k1 public key: generator point G
  static constexpr std::array<uint8_t, 33> kCompressedG{
      0x02, 0x79, 0xBE, 0x66, 0x7E, 0xF9, 0xDC, 0xBB, 0xAC, 0x55, 0xA0,
      0x62, 0x95, 0xCE, 0x87, 0x0B, 0x07, 0x02, 0x9B, 0xFC, 0xDB, 0x2D,
      0xCE, 0x28, 0xD9, 0x59, 0xF2, 0x81, 0x5B, 0x16, 0xF8, 0x17, 0x98};

  Secp256k1PublicKey makeKey() {
    Secp256k1PublicKey k{};
    std::copy(kCompressedG.begin(), kCompressedG.end(), k.begin());
    return k;
  }

}  // namespace

TEST(EnrTest, EncodeDecodeRoundTrip) {
  auto pub = makeKey();
  Port port = 9000;

  auto encoded = lean::enr::encode(pub, port);
  // Encoded string must start with "enr:"
  ASSERT_TRUE(encoded.rfind("enr:", 0) == 0) << encoded;

  auto enr = lean::enr::decode(encoded);

  // Basic fields
  EXPECT_EQ(enr.sequence, 1u);
  EXPECT_EQ(enr.public_key, pub);

  // Signature is all-zero (not actually signed)
  EXPECT_TRUE(std::all_of(enr.signature.begin(),
                          enr.signature.end(),
                          [](uint8_t b) { return b == 0; }));

  // IP and port present and correct
  ASSERT_TRUE(enr.ip.has_value());
  // encode() uses Ip{1,0,0,127}
  Ip expected_ip{1, 0, 0, 127};
  EXPECT_EQ(enr.ip.value(), expected_ip);

  ASSERT_TRUE(enr.port.has_value());
  EXPECT_EQ(enr.port.value(), port);

  // PeerId must be derivable and non-empty
  auto pid = enr.peerId().toBase58();
  EXPECT_FALSE(pid.empty());

  // Connect info wiring
  auto info = enr.connectInfo();
  EXPECT_EQ(info.id, enr.peerId());
}

TEST(EnrTest, DeterministicEncoding) {
  auto pub = makeKey();
  Port port = 12345;
  auto e1 = lean::enr::encode(pub, port);
  auto e2 = lean::enr::encode(pub, port);
  EXPECT_EQ(e1, e2);

  // Changing port changes ENR
  auto e3 = lean::enr::encode(pub, static_cast<Port>(port + 1));
  EXPECT_NE(e1, e3);

  // Changing key changes ENR
  auto pub2 = pub;
  pub2[32] ^= 0x01;  // tweak
  auto e4 = lean::enr::encode(pub2, port);
  EXPECT_NE(e1, e4);
}

TEST(EnrTest, DecodeGivenEnrAddress) {
  // Provided ENR string from user request
  const char *addr =
      "enr:-Ku4QHqVeJ8PPICcWk1vSn_XcSkjOkNiTg6Fmii5j6vUQgvzMc9L1goFnLKgXqBJspJjIsB91LTOleFmyWWrFVATGngBh2F0dG5ldHOIAAAAAAAAAACEZXRoMpC1MD8qAAAAAP__________gmlkgnY0gmlwhAMRHkWJc2VjcDI1NmsxoQKLVXFOhp2uX6jeT0DvvDpPcU8FWMjQdR4wMuORMhpX24N1ZHCCIyg";

  auto enr = lean::enr::decode(addr);

  // Sequence should be positive
  EXPECT_GT(enr.sequence, 0u);

  // IP should be present and equal to 3.17.30.69
  ASSERT_TRUE(enr.ip.has_value());
  Ip expected_ip{3, 17, 30, 69};
  EXPECT_EQ(enr.ip.value(), expected_ip);

  // UDP port should be present; expected commonly used 9000
  ASSERT_TRUE(enr.port.has_value());
  EXPECT_EQ(enr.port.value(), static_cast<Port>(9000));

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
