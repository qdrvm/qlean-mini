/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <system_error>

#include <qtils/byte_vec.hpp>
#include <qtils/bytes.hpp>
#include <sszpp/ssz++.hpp>

namespace lean {

  // template <typename T>
  //  qtils::ByteVec encode(const T &v) {
  //   qtils::ByteVec res;
  //   BOOST_STATIC_ASSERT(sizeof(typename qtils::ByteVec::value_type)
  //                       == sizeof(uint8_t));
  //
  //   auto &as_vec_of_bytes =
  //       *static_cast<std::vector<std::byte> *>(reinterpret_cast<void
  //       *>(&res));
  //   ::ssz::serialize(std::back_inserter(as_vec_of_bytes), v);
  //   return res;
  // }

  template <typename T>
  outcome::result<qtils::ByteVec> encode(const T &v) {
    auto b = ssz::serialize(v);  // std::vector<std::byte>
    qtils::ByteVec out(b.size());
    std::transform(b.begin(), b.end(), out.begin(), [](std::byte x) {
      return std::to_integer<uint8_t>(x);
    });
    return out;
  }

  // template <typename T>
  // inline qtils::ByteVec encode(const T& v) {
  //   std::vector<std::byte> bytes;
  //   ssz::serialize(std::back_inserter(bytes), v);
  //   BOOST_STATIC_ASSERT(sizeof(typename decltype(bytes)::value_type) ==
  //   sizeof(uint8_t)); return std::move(
  //       *static_cast<std::vector<uint8_t> *>(reinterpret_cast<void
  //       *>(&bytes)));
  // }

  template <typename T>
  outcome::result<T> decode(qtils::BytesIn data) {
    try {
      return ssz::deserialize<T>(
          reinterpret_cast<std::span<std::byte> &>(data));
    } catch (...) {
      return std::make_error_code(std::errc::invalid_argument);
    }
  }

  auto sszHash(const auto &v) {
    auto hash1 = ssz::hash_tree_root(v);
    qtils::ByteArr<32> hash2;
    static_assert(hash1.size() == hash2.size());
    memcpy(hash2.data(), hash1.data(), hash1.size());
    return hash2;
  }

  template <size_t N>
  std::array<uint8_t, N>& as_u8(std::array<std::byte, N>& v) {
    return reinterpret_cast<std::array<uint8_t, N>&>(v);
  }

  template <size_t N>
  std::array<std::byte, N>& as_byte(std::array<uint8_t, N>& v) {
    return reinterpret_cast<std::array<std::byte, N>&>(v);
  }

  template <size_t N>
  std::array<uint8_t, N> as_u8(std::array<std::byte, N>&& v) {
    return reinterpret_cast<std::array<uint8_t, N>&>(v);
  }

  template <size_t N>
  std::array<std::byte, N> as_byte(std::array<uint8_t, N>&& v) {
    return reinterpret_cast<std::array<std::byte, N>&>(v);
  }


}  // namespace lean
