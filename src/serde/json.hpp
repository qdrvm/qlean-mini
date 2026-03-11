/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string_view>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include "serde/json_fwd.hpp"

namespace lean::json {
  struct JsonOut {
    RAPIDJSON_DEFAULT_ALLOCATOR &allocator;
    rapidjson::Value &v;
  };

  std::string encode(const auto &v) {
    rapidjson::Document document;
    encode(JsonOut{document.GetAllocator(), document}, v);
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
      json.v.SetUint(v);
    } else {
      json.v.SetInt(v);
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
      encode(JsonOut{json.allocator, json_item}, item);
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
    auto &field_name = field_names.at(I);
    rapidjson::Value json_field;
    encode(JsonOut{json.allocator, json_field}, field);
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
}  // namespace lean::json
