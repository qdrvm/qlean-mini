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

#define _JSON_CAMEL_NAMES_1(name) ::lean::json::toCamelCase(#name)
#define _JSON_CAMEL_NAMES_2(name, ...) \
  _JSON_CAMEL_NAMES_1(name), _JSON_CAMEL_NAMES_1(__VA_ARGS__)
#define _JSON_CAMEL_NAMES_3(name, ...) \
  _JSON_CAMEL_NAMES_1(name), _JSON_CAMEL_NAMES_2(__VA_ARGS__)
#define _JSON_CAMEL_NAMES_4(name, ...) \
  _JSON_CAMEL_NAMES_1(name), _JSON_CAMEL_NAMES_3(__VA_ARGS__)
#define _JSON_CAMEL_NAMES_5(name, ...) \
  _JSON_CAMEL_NAMES_1(name), _JSON_CAMEL_NAMES_4(__VA_ARGS__)
#define _JSON_CAMEL_NAMES_6(name, ...) \
  _JSON_CAMEL_NAMES_1(name), _JSON_CAMEL_NAMES_5(__VA_ARGS__)
#define _JSON_CAMEL_NAMES_7(name, ...) \
  _JSON_CAMEL_NAMES_1(name), _JSON_CAMEL_NAMES_6(__VA_ARGS__)
#define _JSON_CAMEL_NAMES_8(name, ...) \
  _JSON_CAMEL_NAMES_1(name), _JSON_CAMEL_NAMES_7(__VA_ARGS__)
#define _JSON_CAMEL_NAMES_9(name, ...) \
  _JSON_CAMEL_NAMES_1(name), _JSON_CAMEL_NAMES_8(__VA_ARGS__)
#define _JSON_CAMEL_NAMES_10(name, ...) \
  _JSON_CAMEL_NAMES_1(name), _JSON_CAMEL_NAMES_9(__VA_ARGS__)
#define _JSON_CAMEL_NAMES_OVERLOAD(                      \
    _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, macro, ...) \
  macro
#define _JSON_CAMEL_NAMES_OVERLOAD_CALL(macro, ...) macro(__VA_ARGS__)
#define _JSON_CAMEL_NAMES(...)                         \
  _JSON_CAMEL_NAMES_OVERLOAD_CALL(                     \
      _JSON_CAMEL_NAMES_OVERLOAD(__VA_ARGS__,          \
                                 _JSON_CAMEL_NAMES_10, \
                                 _JSON_CAMEL_NAMES_9,  \
                                 _JSON_CAMEL_NAMES_8,  \
                                 _JSON_CAMEL_NAMES_7,  \
                                 _JSON_CAMEL_NAMES_6,  \
                                 _JSON_CAMEL_NAMES_5,  \
                                 _JSON_CAMEL_NAMES_4,  \
                                 _JSON_CAMEL_NAMES_3,  \
                                 _JSON_CAMEL_NAMES_2,  \
                                 _JSON_CAMEL_NAMES_1), \
      __VA_ARGS__)

#define JSON_CAMEL(...)                                            \
  static const auto &fieldNames() {                                \
    static std::array field_names{_JSON_CAMEL_NAMES(__VA_ARGS__)}; \
    return field_names;                                            \
  }                                                                \
  auto fields() {                                                  \
    return std::tie(__VA_ARGS__);                                  \
  }

#define JSON_CAMEL_DISCRIMINATOR(type_field_name, ...)             \
  static const std::string &typeFieldName() {                      \
    static auto name = ::lean::json::toCamelCase(type_field_name); \
    return name;                                                   \
  }                                                                \
  static const auto &typeFieldValues() {                           \
    static std::array values{__VA_ARGS__};                         \
    return values;                                                 \
  }

#define JSON_ENUM(type, ...)                                              \
  inline const auto &enumValues(const type &) {                           \
    static std::vector<std::pair<type, std::string>> values{__VA_ARGS__}; \
    return values;                                                        \
  }

namespace lean::json {
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
}  // namespace lean::json
