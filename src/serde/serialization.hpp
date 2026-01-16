/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <system_error>

#include <qtils/byte_vec.hpp>
#include <qtils/bytes.hpp>
#include <qtils/enum_error_code.hpp>
#include <sszpp/ssz++.hpp>

namespace lean {
  enum class SszError {
    DecodeError,
  };
  Q_ENUM_ERROR_CODE(SszError) {
    using E = decltype(e);
    switch (e) {
      case E::DecodeError:
        return "ssz decode error";
    }
    abort();
  }

  template <typename T>
  outcome::result<qtils::ByteVec> encode(const T &v) {
    auto b = ssz::serialize(v);  // std::vector<std::byte>
    qtils::ByteVec out(b.size());
    std::transform(b.begin(), b.end(), out.begin(), [](std::byte x) {
      return std::to_integer<uint8_t>(x);
    });
    return out;
  }

  template <typename T>
  outcome::result<T> decode(qtils::BytesIn data) {
    try {
      return ssz::deserialize<T>(
          reinterpret_cast<std::span<std::byte> &>(data));
    } catch (const std::out_of_range &) {
      return outcome::failure(SszError::DecodeError);
    } catch (const std::invalid_argument &) {
      return outcome::failure(SszError::DecodeError);
    }
  }

  auto sszHash(const auto &v) {
    const auto hash_tree_root = ssz::hash_tree_root(v);

    using Src = decltype(hash_tree_root);
    using Dst = qtils::ByteArr<32>;

    static_assert(sizeof(Src) == sizeof(Dst));
    static_assert(alignof(Src) == alignof(Dst));
    static_assert(std::is_trivially_copyable_v<Src>);
    static_assert(std::is_trivially_copyable_v<Dst>);

    return std::bit_cast<Dst>(hash_tree_root);
  }

  template <size_t N>
  std::array<uint8_t, N> &as_u8(std::array<std::byte, N> &v) {
    return reinterpret_cast<std::array<uint8_t, N> &>(v);
  }

  template <size_t N>
  std::array<std::byte, N> &as_byte(std::array<uint8_t, N> &v) {
    return reinterpret_cast<std::array<std::byte, N> &>(v);
  }

  template <size_t N>
  std::array<uint8_t, N> as_u8(std::array<std::byte, N> &&v) {
    return reinterpret_cast<std::array<uint8_t, N> &>(v);
  }

  template <size_t N>
  std::array<std::byte, N> as_byte(std::array<uint8_t, N> &&v) {
    return reinterpret_cast<std::array<std::byte, N> &>(v);
  }


}  // namespace lean
