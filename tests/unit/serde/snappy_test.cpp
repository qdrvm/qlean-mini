/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "serde/snappy.hpp"

#include <gtest/gtest.h>

TEST(SnappyTest, Framed) {
  auto compressed =
      qtils::ByteVec::fromHex(
          "ff060000734e61507059002f00009243dc2a5080215db8fcf7f8ea795d9c370d0654"
          "cc7822936053f7a0b583213792bd483191e1000d0104215d962800")
          .value();
  auto uncompressed = lean::snappy::uncompressFramed(compressed).value();
  auto compressed2 = lean::snappy::compressFramed(uncompressed);
  // c++ compressor may return different bytes than rust compressor
  auto uncompressed2 = lean::snappy::uncompressFramed(compressed2).value();
  EXPECT_EQ(uncompressed2, uncompressed);
}
