/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <ranges>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <yaml-cpp/yaml.h>

namespace lean::yaml {
  struct Node {
    [[noreturn]] void error() const {
      throw std::runtime_error{
          fmt::format("{} {}", file_path, fmt::join(keys, ""))};
    }

    Node map(std::string key) const {
      if (not yaml.IsMap()) {
        error();
      }
      Node node{
          .file_path = file_path,
          .keys = keys,
          .yaml = yaml[key],
      };
      node.keys.emplace_back(fmt::format("[\"{}\"]", key));
      return node;
    }

    auto map() const {
      if (not yaml.IsMap()) {
        error();
      }
      return yaml | std::views::transform([&](auto &&t) {
               auto key = t.first.template as<std::string>();
               Node node{
                   .file_path = file_path,
                   .keys = keys,
                   .yaml = t.second,
               };
               node.keys.emplace_back(fmt::format("[\"{}\"]", key));
               return std::make_pair(std::move(key), std::move(node));
             });
    }

    auto list() const {
      if (not yaml.IsSequence()) {
        error();
      }
      return std::views::zip(std::views::iota(size_t{0}, yaml.size()), yaml)
           | std::views::transform([&](auto &&t) {
               auto &&[i, yaml] = t;
               Node node{
                   .file_path = file_path,
                   .keys = keys,
                   .yaml = yaml,
               };
               node.keys.emplace_back(fmt::format("[{}]", i));
               return node;
             });
    }

    auto str() const {
      if (not yaml.IsScalar()) {
        error();
      }
      return yaml.as<std::string>();
    }

    template <typename T>
    T num() const {
      if (not yaml.IsScalar()) {
        error();
      }
      return yaml.as<T>();
    }

    std::string file_path;
    std::vector<std::string> keys;
    YAML::Node yaml;
  };

  inline Node read(std::string file_path) {
    auto yaml = YAML::LoadFile(file_path);
    return Node{
        .file_path = file_path,
        .keys = {},
        .yaml = yaml,
    };
  }
}  // namespace lean::yaml
