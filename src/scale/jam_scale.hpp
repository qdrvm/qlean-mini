/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <scale/scale.hpp>

namespace lean {
  using scale::impl::memory::decode;
  using scale::impl::memory::encode;

  template <typename T, typename... Configs>
  [[nodiscard]] outcome::result<std::vector<uint8_t>> encode_with_config(
      T &&value, Configs &&...configs) {
    std::vector<uint8_t> out;
    scale::backend::ToBytes encoder(out, std::forward<Configs>(configs)...);
    try {
      encode(std::forward<T>(value), encoder);
    } catch (std::system_error &e) {
      return outcome::failure(e.code());
    }
    return std::move(out);
  }

  template <typename T, typename... Configs>
  [[nodiscard]] outcome::result<T> decode_with_config(
      const auto &bytes, Configs &&...configs) {
    scale::backend::FromBytes decoder(bytes, std::forward<Configs>(configs)...);
    T value;
    try {
      decode(value, decoder);
    } catch (std::system_error &e) {
      return outcome::failure(e.code());
    }
    return std::move(value);
  }

}  // namespace lean
