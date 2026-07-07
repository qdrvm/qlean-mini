/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <qtils/test/outcome.hpp>

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
  std::visit(
      [&](auto &v) {
        auto encoded_res = lean::encode(v.value);
        auto decoded_res =
            lean::decode<typename std::remove_cvref_t<decltype(v)>::Type>(
                v.serialized);
        if (v.expect_exception.has_value()) {
          if (encoded_res.has_value()) {
            auto &encoded = encoded_res.value();
            EXPECT_NE(encoded.toHex(), v.serialized.toHex());
          }
          ASSERT_OUTCOME_ERROR(decoded_res);
        } else {
          ASSERT_OUTCOME_SUCCESS(encoded, encoded_res);
          EXPECT_EQ(encoded.toHex(), v.serialized.toHex());
          ASSERT_OUTCOME_SUCCESS(decoded_res);
        }
      },
      fixture.v);
}
