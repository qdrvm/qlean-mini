/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <array>
#include <string>
#include <tuple>
#include <vector>

#define _JSON_FIELDS_1(name) std::string(#name)
#define _JSON_FIELDS_2(name, ...) \
  _JSON_FIELDS_1(name), _JSON_FIELDS_1(__VA_ARGS__)
#define _JSON_FIELDS_3(name, ...) \
  _JSON_FIELDS_1(name), _JSON_FIELDS_2(__VA_ARGS__)
#define _JSON_FIELDS_4(name, ...) \
  _JSON_FIELDS_1(name), _JSON_FIELDS_3(__VA_ARGS__)
#define _JSON_FIELDS_5(name, ...) \
  _JSON_FIELDS_1(name), _JSON_FIELDS_4(__VA_ARGS__)
#define _JSON_FIELDS_6(name, ...) \
  _JSON_FIELDS_1(name), _JSON_FIELDS_5(__VA_ARGS__)
#define _JSON_FIELDS_7(name, ...) \
  _JSON_FIELDS_1(name), _JSON_FIELDS_6(__VA_ARGS__)
#define _JSON_FIELDS_8(name, ...) \
  _JSON_FIELDS_1(name), _JSON_FIELDS_7(__VA_ARGS__)
#define _JSON_FIELDS_9(name, ...) \
  _JSON_FIELDS_1(name), _JSON_FIELDS_8(__VA_ARGS__)
#define _JSON_FIELDS_10(name, ...) \
  _JSON_FIELDS_1(name), _JSON_FIELDS_9(__VA_ARGS__)
#define _JSON_FIELDS_OVERLOAD(                           \
    _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, macro, ...) \
  macro
#define _JSON_FIELDS_OVERLOAD_CALL(macro, ...) macro(__VA_ARGS__)
#define _JSON_FIELDS(...)                                           \
  _JSON_FIELDS_OVERLOAD_CALL(_JSON_FIELDS_OVERLOAD(__VA_ARGS__,     \
                                                   _JSON_FIELDS_10, \
                                                   _JSON_FIELDS_9,  \
                                                   _JSON_FIELDS_8,  \
                                                   _JSON_FIELDS_7,  \
                                                   _JSON_FIELDS_6,  \
                                                   _JSON_FIELDS_5,  \
                                                   _JSON_FIELDS_4,  \
                                                   _JSON_FIELDS_3,  \
                                                   _JSON_FIELDS_2,  \
                                                   _JSON_FIELDS_1), \
                             __VA_ARGS__)

#define JSON_FIELDS(...)                                            \
  static const auto &fieldNames() {                                 \
    static const std::array field_names{_JSON_FIELDS(__VA_ARGS__)}; \
    return field_names;                                             \
  }                                                                 \
  auto fields() const {                                             \
    return std::tie(__VA_ARGS__);                                   \
  }
