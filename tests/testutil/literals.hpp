/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdexcept>

#include <qtils/literals.hpp>

#define CREATE_LITERAL(N)                                                   \
  consteval qtils::ByteArr<N> operator""_arr##N(const char *c, size_t s) {  \
    if (s > N) throw std::invalid_argument("Literal too long for ByteArr"); \
    qtils::ByteArr<N> arr{};                                                \
    std::copy_n(c, s, arr.begin());                                         \
    return arr;                                                             \
  }

CREATE_LITERAL(8)
CREATE_LITERAL(16)
CREATE_LITERAL(32)

#undef CREATE_LITERAL
