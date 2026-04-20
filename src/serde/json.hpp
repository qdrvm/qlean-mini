/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <concepts>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include <qtils/byte_arr.hpp>
#include <qtils/byte_vec.hpp>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <sszpp/lists.hpp>

#include "serde/json_fwd.hpp"

#define JSON_ASSERT(c) \
  if (not(c)) json.error()

namespace lean::json {
  struct JsonOut {
    JsonOut(NameCase name_case,
            rapidjson::Value &v,
            RAPIDJSON_DEFAULT_ALLOCATOR &allocator)
        : name_case{name_case}, v{v}, allocator{allocator} {}

    JsonOut(const JsonOut &json, rapidjson::Value &v)
        : name_case{json.name_case}, v{v}, allocator{json.allocator} {}

    NameCase name_case;
    rapidjson::Value &v;
    RAPIDJSON_DEFAULT_ALLOCATOR &allocator;
  };

  struct JsonIn {
    JsonIn(NameCase name_case, const rapidjson::Value &v)
        : name_case{name_case}, v{v} {}

    JsonIn(const JsonIn &json, std::string key, const rapidjson::Value &v)
        : name_case{json.name_case}, v{v}, keys{json.keys} {
      keys.emplace_back(key);
    }

    [[noreturn]] void error() {
      throw std::runtime_error{fmt::format("json: {}", fmt::join(keys, ""))};
    }

    NameCase name_case;
    const rapidjson::Value &v;
    std::vector<std::string> keys;
  };

  std::string encode(NameCase name_case, const auto &v) {
    rapidjson::Document document;
    encode(JsonOut{name_case, document, document.GetAllocator()}, v);
    rapidjson::StringBuffer stream;
    rapidjson::Writer writer{stream};
    document.Accept(writer);
    return std::string{stream.GetString(), stream.GetSize()};
  }

  template <std::integral T>
  void encode(JsonOut json, const T &v) {
    if constexpr (std::is_same_v<T, bool>) {
      json.v.SetBool(v);
    } else if constexpr (std::is_unsigned_v<T>) {
      json.v.SetUint64(v);
    } else {
      json.v.SetInt64(v);
    }
  }

  inline void encode(JsonOut json, std::string_view v) {
    json.v.SetString(v.data(), v.size(), json.allocator);
  }

  template <typename T>
  void encode(JsonOut json, const std::vector<T> &v) {
    json.v.SetArray();
    for (auto &item : v) {
      rapidjson::Value json_item;
      encode(JsonOut{json, json_item}, item);
      json.v.PushBack(std::move(json_item), json.allocator);
    }
  }

  template <size_t N>
  void encode(JsonOut json, const qtils::ByteArr<N> &v) {
    encode(json, fmt::format("{:0xx}", qtils::Hex{v}));
  }

  template <size_t I, typename T>
  void encodeFields(JsonOut json, const T &fields, const auto &field_names) {
    auto &field = std::get<I>(fields);
    auto &field_name = field_names.at(I)[json.name_case];
    rapidjson::Value json_field;
    encode(JsonOut{json, json_field}, field);
    json.v.AddMember(rapidjson::StringRef(field_name.data(), field_name.size()),
                     std::move(json_field),
                     json.allocator);
    if constexpr (I + 1 < std::tuple_size_v<T>) {
      encodeFields<I + 1>(json, fields, field_names);
    }
  }

  template <typename T>
    requires requires(const T &v) {
      v.fieldNames();
      v.fields();
    }
  void encode(JsonOut json, const T &v) {
    auto fields = v.fields();
    auto &field_names = v.fieldNames();
    json.v.SetObject();
    encodeFields<0>(json, fields, field_names);
  }

  void decode(NameCase name_case, auto &v, std::string_view json_str) {
    rapidjson::Document document;
    document.Parse(json_str.data(), json_str.size());
    decode(JsonIn{name_case, document}, v);
  }

  template <typename T>
  T overflow(JsonIn json, auto v) {
    JSON_ASSERT(v >= std::numeric_limits<T>::min());
    JSON_ASSERT(v <= std::numeric_limits<T>::max());
    return v;
  };

  template <std::integral T>
  void decode(JsonIn json, T &v) {
    if constexpr (std::is_same_v<T, bool>) {
      JSON_ASSERT(json.v.IsBool());
      v = json.v.GetBool();
    } else if constexpr (std::is_unsigned_v<T>) {
      JSON_ASSERT(json.v.IsUint64());
      v = overflow<T>(json, json.v.GetUint64());
    } else {
      JSON_ASSERT(json.v.IsInt64());
      v = overflow<T>(json, json.v.GetInt64());
    }
  }

  inline std::string_view decodeStr(JsonIn json) {
    JSON_ASSERT(json.v.IsString());
    return {json.v.GetString(), json.v.GetStringLength()};
  }

  inline void decode(JsonIn json, std::string &v) {
    v = decodeStr(json);
  }

  inline void decode(JsonIn json, qtils::ByteVec &v) {
    v = qtils::unhex0x<qtils::ByteVec>(decodeStr(json)).value();
  }

  template <size_t N>
  void decode(JsonIn json, qtils::ByteArr<N> &v) {
    v = qtils::ByteArr<N>::fromHexWithPrefix(decodeStr(json)).value();
  }

  template <typename T>
  void decode(JsonIn json, std::vector<T> &v) {
    v.clear();
    JSON_ASSERT(json.v.IsArray());
    size_t i = 0;
    for (auto it = json.v.Begin(); it != json.v.End(); ++it, ++i) {
      T value;
      decode(JsonIn{json, fmt::format("[{}]", i), *it}, value);
      v.emplace_back(std::move(value));
    }
  }

  template <typename T>
  void decode(JsonIn json, std::optional<T> &v) {
    v.reset();
    if (not json.v.IsNull()) {
      T value;
      decode(json, value);
      v.emplace(std::move(value));
    }
  }

  template <typename T>
  void decode(JsonIn json, std::unordered_map<std::string, T> &v) {
    v.clear();
    JSON_ASSERT(json.v.IsObject());
    for (auto it = json.v.MemberBegin(); it != json.v.MemberEnd(); ++it) {
      std::string key;
      decode(JsonIn{json, "(key)", it->name}, key);
      T value;
      decode(JsonIn{json, fmt::format("[\"{}\"]", key), it->value}, value);
      v.emplace(std::move(key), std::move(value));
    }
  }

  template <typename T, size_t N>
  void decode(JsonIn json, ssz::list<T, N> &v) {
    static std::array<FieldName, 1> field_names{FieldName{"data"}};
    decodeFields<0>(json, std::tie(v.data()), field_names);
  }

  template <size_t I, typename T>
  void decodeFields(JsonIn json, const T &fields, const auto &field_names) {
    JSON_ASSERT(json.v.IsObject());
    auto &field = std::get<I>(fields);
    auto &field_name = field_names.at(I)[json.name_case];
    auto it = json.v.FindMember(field_name.c_str());
    static const rapidjson::Value json_null;
    decode(JsonIn{json,
                  fmt::format("[\"{}\"]", field_name),
                  it != json.v.MemberEnd() ? it->value : json_null},
           field);
    if constexpr (I + 1 < std::tuple_size_v<T>) {
      decodeFields<I + 1>(json, fields, field_names);
    }
  }

  template <typename T>
    requires requires(T &v) {
      v.fieldNames();
      v.fields();
    }
  void decode(JsonIn json, T &v) {
    auto fields = v.fields();
    auto &field_names = v.fieldNames();
    decodeFields<0>(json, fields, field_names);
  }

  template <size_t I, typename... T>
  void decodeDiscriminator(JsonIn json,
                           std::variant<T...> &v,
                           const auto &type_field_values,
                           const std::string &type_field_value) {
    if (type_field_value == type_field_values.at(I)) {
      decode(json, v.template emplace<I>());
      return;
    }
    if constexpr (I + 1 < sizeof...(T)) {
      decodeDiscriminator<I + 1>(json, v, type_field_values, type_field_value);
    } else {
      JSON_ASSERT(false);
    }
  }

  template <typename T>
    requires requires(T &v) {
      v.typeFieldName();
      v.typeFieldValues();
    }
  void decode(JsonIn json, T &v) {
    auto &type_field_name = v.typeFieldName();
    auto &type_field_values = v.typeFieldValues();
    static std::array<FieldName, 1> field_names{type_field_name};
    std::string type_field_value;
    decodeFields<0>(json, std::tie(type_field_value), field_names);
    decodeDiscriminator<0>(json, v.v, type_field_values, type_field_value);
  }

  template <typename T>
    requires requires(T &v) { enumValues(v); }
  void decode(JsonIn json, T &v) {
    auto &enum_values = enumValues(v);
    auto str = decodeStr(json);
    for (auto &[enum_value, enum_str] : enum_values) {
      if (str == enum_str) {
        v = enum_value;
        return;
      }
    }
    JSON_ASSERT(false);
  }

  template <typename T>
    requires requires(T &v) { v.wrappedField(); }
  void decode(JsonIn json, T &v) {
    decode(json, v.wrappedField());
  }

  template <size_t N>
  void decode(JsonIn json, ssz::list<uint8_t, N> &v) {
    static std::array<FieldName, 1> field_names{FieldName{"data"}};
    qtils::ByteVec bytes;
    decodeFields<0>(json, std::tie(bytes), field_names);
    v.data() = std::move(bytes);
  }
}  // namespace lean::json
