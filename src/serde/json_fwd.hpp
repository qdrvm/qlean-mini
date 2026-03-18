/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <array>
#include <cctype>
#include <string>
#include <tuple>
#include <vector>

#define _JSON_FIELDS_1(name) ::lean::json::FieldName(#name)
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
#define _JSON_FIELDS_11(name, ...) \
  _JSON_FIELDS_1(name), _JSON_FIELDS_10(__VA_ARGS__)
#define _JSON_FIELDS_12(name, ...) \
  _JSON_FIELDS_1(name), _JSON_FIELDS_11(__VA_ARGS__)
#define _JSON_FIELDS_13(name, ...) \
  _JSON_FIELDS_1(name), _JSON_FIELDS_12(__VA_ARGS__)
#define _JSON_FIELDS_14(name, ...) \
  _JSON_FIELDS_1(name), _JSON_FIELDS_13(__VA_ARGS__)
#define _JSON_FIELDS_15(name, ...) \
  _JSON_FIELDS_1(name), _JSON_FIELDS_14(__VA_ARGS__)
#define _JSON_FIELDS_16(name, ...) \
  _JSON_FIELDS_1(name), _JSON_FIELDS_15(__VA_ARGS__)
#define _JSON_FIELDS_17(name, ...) \
  _JSON_FIELDS_1(name), _JSON_FIELDS_16(__VA_ARGS__)
#define _JSON_FIELDS_OVERLOAD(_1,    \
                              _2,    \
                              _3,    \
                              _4,    \
                              _5,    \
                              _6,    \
                              _7,    \
                              _8,    \
                              _9,    \
                              _10,   \
                              _11,   \
                              _12,   \
                              _13,   \
                              _14,   \
                              _15,   \
                              _16,   \
                              _17,   \
                              macro, \
                              ...)   \
  macro
#define _JSON_FIELDS_OVERLOAD_CALL(macro, ...) macro(__VA_ARGS__)
#define _JSON_FIELDS(...)                                           \
  _JSON_FIELDS_OVERLOAD_CALL(_JSON_FIELDS_OVERLOAD(__VA_ARGS__,     \
                                                   _JSON_FIELDS_17, \
                                                   _JSON_FIELDS_16, \
                                                   _JSON_FIELDS_15, \
                                                   _JSON_FIELDS_14, \
                                                   _JSON_FIELDS_13, \
                                                   _JSON_FIELDS_12, \
                                                   _JSON_FIELDS_11, \
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
  }                                                                 \
  auto fields() {                                                   \
    return std::tie(__VA_ARGS__);                                   \
  }

#define JSON_DISCRIMINATOR(type_field_name, ...)        \
  static const auto &typeFieldName() {                  \
    static auto name = _JSON_FIELDS_1(type_field_name); \
    return name;                                        \
  }                                                     \
  static const auto &typeFieldValues() {                \
    static std::array values{__VA_ARGS__};              \
    return values;                                      \
  }

#define JSON_ENUM(type, ...)                                              \
  inline const auto &enumValues(const type &) {                           \
    static std::vector<std::pair<type, std::string>> values{__VA_ARGS__}; \
    return values;                                                        \
  }

#define JSON_WRAPPER(field)    \
  auto &wrappedField() const { \
    return field;              \
  }                            \
  auto &wrappedField() {       \
    return field;              \
  }

#define SSZ_AND_JSON_FIELDS(...) \
  SSZ_CONT(__VA_ARGS__);         \
  JSON_FIELDS(__VA_ARGS__)

#define SSZ_AND_JSON_WRAPPER(field) \
  SSZ_WRAPPER(field);               \
  JSON_WRAPPER(field)

namespace lean::json {
  enum NameCase { SNAKE, CAMEL };

  inline std::string toCamelCase(std::string_view name) {
    std::string camel;
    bool capitalize = false;
    for (auto &c : name) {
      if (c == '_') {
        capitalize = true;
        continue;
      }
      if (capitalize) {
        capitalize = false;
        camel.push_back(std::toupper(c));
      } else {
        camel.push_back(c);
      }
    }
    return camel;
  }

  struct FieldName {
    FieldName(std::string snake)
        : snake{std::move(snake)}, camel{toCamelCase(this->snake)} {}

    const std::string &operator[](NameCase name_case) const {
      switch (name_case) {
        case NameCase::SNAKE:
          return snake;
        case NameCase::CAMEL:
          return camel;
      }
    }

    std::string snake;
    std::string camel;
  };
}  // namespace lean::json
