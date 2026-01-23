/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/container.hpp>

#define SSZ_WRAPPER(field)                                                 \
  constexpr std::size_t ssz_size() const noexcept {                        \
    return ssz::size(field);                                               \
  }                                                                        \
  constexpr void serialize(ssz::ssz_iterator auto result) const {          \
    ssz::serialize(result, field);                                         \
  }                                                                        \
  constexpr void deserialize(const std::ranges::sized_range auto &bytes) { \
    ssz::deserialize(bytes, field);                                        \
  }                                                                        \
  void hash_tree_root(ssz::ssz_iterator auto result, size_t cpu_count = 0) \
      const {                                                              \
    ssz::hash_tree_root(result, field, cpu_count);                         \
  }                                                                        \
  void assert_consistent_variable_size() const {                           \
    auto t = std::tie(field);                                              \
    static_assert(variable_size::value                                     \
                  or ssz::tuple_all_fixed_size<decltype(t)>::value);       \
  }
