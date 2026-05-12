/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ssz_test_json.hpp"
#include "test_vectors.hpp"

struct SszTest : FixtureTest<lean::SszTestJson> {};
FIXTURE_INSTANTIATE(SszTest, "ssz");

TEST_P(SszTest, Ssz) {
  auto &[name, fixture] = GetParam();
  std::println("RUN {}", name);
  std::println("  TYPE {}", fixture.typeName());
  if (fixture.disabled()) {
    std::println("  DISABLED");
    return;
  }
  auto [actual, expected] = std::visit(
      [&](auto &v) {
        return std::make_pair(lean::encode(v.value).value(), v.serialized);
      },
      fixture.v);
  EXPECT_EQ(actual.toHex(), expected.toHex());
}
