/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string_view>

#include <rapidjson/document.h>
#include <sszpp/lists.hpp>

#include "serde/json_fwd.hpp"
#include "types/state.hpp"

#define JSON_ASSERT(c) \
  if (not(c)) throw std::runtime_error{"json"}

namespace lean::json {
  struct Json {
    const rapidjson::Value &v;
  };

  void decode(auto &v, std::string_view json_str) {
    rapidjson::Document document;
    document.Parse(json_str.data(), json_str.size());
    decode(v, Json{document});
  }

  inline std::string_view decodeStr(Json json) {
    JSON_ASSERT(json.v.IsString());
    return {json.v.GetString(), json.v.GetStringLength()};
  }

  inline void decode(std::string &v, Json json) {
    v = decodeStr(json);
  }

  template <typename T>
  void decode(std::unordered_map<std::string, T> &v, Json json) {
    v.clear();
    JSON_ASSERT(json.v.IsObject());
    for (auto it = json.v.MemberBegin(); it != json.v.MemberEnd(); ++it) {
      std::string key;
      decode(key, Json{it->name});
      T value;
      decode(value, Json{it->value});
      v.emplace(std::move(key), std::move(value));
    }
  }

  template <typename T>
  void decode(std::optional<T> &v, Json json) {
    v.reset();
    if (not json.v.IsNull()) {
      T value;
      decode(value, json);
      v.emplace(std::move(value));
    }
  }

  template <typename T>
  void decode(std::vector<T> &v, Json json) {
    v.clear();
    JSON_ASSERT(json.v.IsArray());
    for (auto it = json.v.Begin(); it != json.v.End(); ++it) {
      T value;
      decode(value, Json{*it});
      v.emplace_back(std::move(value));
    }
  }

  template <std::integral T>
  void decode(T &v, Json json) {
    if constexpr (std::is_unsigned_v<T>) {
      JSON_ASSERT(json.v.IsUint());
      v = json.v.GetUint();
    } else {
      JSON_ASSERT(json.v.IsInt());
      v = json.v.GetInt();
    }
  }

  template <size_t I, typename T>
  void decodeFields(const T &fields, const auto &field_names, Json json) {
    JSON_ASSERT(json.v.IsObject());
    auto &field = std::get<I>(fields);
    auto &field_name = field_names.at(I);
    auto it = json.v.FindMember(field_name.c_str());
    static const rapidjson::Value json_null;
    decode(field, Json{it != json.v.MemberEnd() ? it->value : json_null});
    if constexpr (I + 1 < std::tuple_size_v<T>) {
      decodeFields<I + 1>(fields, field_names, json);
    }
  }

  template <typename T>
    requires requires(T &v) {
      v.fieldNames();
      v.fields();
    }
  void decode(T &v, Json json) {
    auto fields = v.fields();
    auto field_names = v.fieldNames();
    decodeFields<0>(fields, field_names, json);
  }

  template <size_t N>
  void decode(qtils::ByteArr<N> &v, Json json) {
    v = qtils::ByteArr<N>::fromHexWithPrefix(decodeStr(json)).value();
  }

  template <typename T, size_t N>
  void decode(ssz::list<T, N> &v, Json json) {
    static std::array<std::string, 1> field_names{"data"};
    decodeFields<0>(std::tie(v.data()), field_names, json);
  }
}  // namespace lean::json
