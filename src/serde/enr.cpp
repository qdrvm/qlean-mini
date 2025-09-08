/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "serde/enr.hpp"

#include <bit>

#include <cppcodec/base64_url_unpadded.hpp>
#include <libp2p/peer/peer_id_from.hpp>
#include <qtils/bytestr.hpp>

namespace lean::rlp {
  constexpr uint8_t kMaxPrefix1 = 55;
  constexpr uint8_t kBytesPrefix1 = 0x80;
  constexpr uint8_t kListPrefix1 = 0xc0;

  struct Decoder {
    qtils::BytesIn input_;

    bool empty() const {
      return input_.empty();
    }

    qtils::BytesIn _take(size_t n) {
      assert(n <= input_.size());
      auto r = input_.first(n);
      input_ = input_.subspan(n);
      return r;
    }

    template <typename T>
    static T _uint(qtils::BytesIn be) {
      T v = 0;
      for (auto &x : be) {
        v = (v << 8) | x;
      }
      return v;
    }

    template <uint8_t base1>
    qtils::BytesIn _bytes() {
      constexpr auto base2 = base1 + kMaxPrefix1;
      assert(base1 <= input_[0]);
      if (input_[0] <= base2) {
        auto n = input_[0] - base1;
        _take(1);
        return _take(n);
      }
      auto n1 = input_[0] - base2;
      _take(1);
      auto n2 = _uint<size_t>(_take(n1));
      return _take(n2);
    }

    bool is_list() const {
      return not empty() and kListPrefix1 <= input_[0];
    }

    Decoder list() {
      assert(not empty());
      return Decoder{_bytes<kListPrefix1>()};
    }

    qtils::BytesIn bytes() {
      assert(not empty());
      assert(input_[0] < kListPrefix1);
      if (input_[0] < kBytesPrefix1) {
        return _take(1);
      }
      return _bytes<kBytesPrefix1>();
    }

    template <typename T>
    T bytes_n() {
      auto raw = bytes();
      T r;
      qtils::BytesOut{r};
      assert(raw.size() == r.size());
      memcpy(r.data(), raw.data(), r.size());
      return r;
    }

    template <size_t N>
    qtils::ByteArr<N> bytes_n() {
      return bytes_n<qtils::ByteArr<N>>();
    }

    std::string_view str() {
      return qtils::byte2str(bytes());
    }

    void str(std::string_view expected) {
      auto actual = str();
      assert(actual == expected);
    }

    template <typename T>
    T uint() {
      auto be = bytes();
      return _uint<T>(be);
    }

    void skip() {
      if (is_list()) {
        list();
      } else {
        bytes();
      }
    }
  };

  struct EncodeBuffer {
    qtils::ByteArr<1 + 8> buffer_{};
    uint8_t size_ = 0;

    operator qtils::BytesIn() const {
      return qtils::BytesIn{buffer_}.first(size_);
    }
    bool is_short() const {
      return buffer_[0] < kBytesPrefix1;
    }
    template <uint8_t base>
    void _uint(uint64_t v) {
      auto n = sizeof(uint64_t) - std::countl_zero(v) / 8;
      size_ = 1 + n;
      buffer_[0] = base + n;
      for (size_t i = 1; i < size_; ++i) {
        buffer_[i] = v >> (8 * (n - i));
      }
    }
    template <uint8_t base>
    void _bytes(size_t bytes) {
      if (bytes <= kMaxPrefix1) {
        size_ = 1;
        buffer_[0] = base + bytes;
      } else {
        _uint<base + kMaxPrefix1>(bytes);
      }
    }
  };

  struct EncodeUint : EncodeBuffer {
    EncodeUint(uint64_t v) {
      if (0 < v and v < kBytesPrefix1) {
        size_ = 1;
        buffer_[0] = v;
      } else {
        _uint<kBytesPrefix1>(v);
      }
    }
  };

  struct EncodeBytes : EncodeBuffer {
    EncodeBytes(qtils::BytesIn bytes) {
      if (bytes.size() == 1 and bytes[0] < kBytesPrefix1) {
        size_ = 1;
        buffer_[0] = bytes[0];
      } else {
        _bytes<kBytesPrefix1>(bytes.size());
      }
    }
  };

  struct EncodeList : EncodeBuffer {
    EncodeList(size_t bytes) {
      _bytes<kListPrefix1>(bytes);
    }
  };

  struct Encoder {
    qtils::ByteVec output_;

    void uint(uint64_t v) {
      output_.put(EncodeUint{v});
    }

    void bytes(qtils::BytesIn bytes) {
      EncodeBytes prefix{bytes};
      output_.put(prefix);
      if (not prefix.is_short()) {
        output_.put(bytes);
      }
    }

    void str(std::string_view s) {
      bytes(qtils::str2byte(s));
    }

    qtils::ByteVec list() {
      qtils::ByteVec r;
      r.put(EncodeList{output_.size()});
      r.put(output_);
      return r;
    }
  };
}  // namespace lean::rlp

namespace lean::enr {
  libp2p::PeerId Enr::peerId() const {
    return libp2p::peerIdFromSecp256k1(public_key);
  }

  libp2p::Multiaddress Enr::listen() const {
    return libp2p::Multiaddress::create(
               std::format("/ip4/0.0.0.0/udp/{}/quic-v1", port.value()))
        .value();
  }

  libp2p::Multiaddress Enr::connect() const {
    auto &ip = this->ip.value();
    return libp2p::Multiaddress::create(
               std::format("/ip4/{}.{}.{}.{}/udp/{}/quic-v1/p2p/{}",
                           ip[3],
                           ip[2],
                           ip[1],
                           ip[0],
                           port.value(),
                           peerId().toBase58()))
        .value();
  }

  Enr decode(std::string_view str) {
    constexpr std::string_view s_enr{"enr:"};
    assert(str.starts_with(s_enr));
    str.remove_prefix(s_enr.size());
    auto rlp_bytes = cppcodec::base64_url_unpadded::decode(str);
    rlp::Decoder rlp{rlp_bytes};
    rlp = rlp.list();
    Enr enr;
    enr.signature = rlp.bytes_n<Secp256k1Signature>();
    enr.sequence = rlp.uint<Sequence>();
    std::string_view key;
    key = rlp.str();
    while (key != "id") {
      rlp.skip();
      key = rlp.str();
    }
    assert(key == "id");
    rlp.str("v4");
    key = rlp.str();
    if (key == "ip") {
      enr.ip = rlp.bytes_n<Ip>();
      key = rlp.str();
    }
    assert(key == "secp256k1");
    enr.public_key = rlp.bytes_n<Secp256k1PublicKey>();
    if (not rlp.empty()) {
      rlp.str("udp");
      enr.port = rlp.uint<Port>();
    }
    assert(rlp.empty());
    return enr;
  }

  std::string encode(const Secp256k1PublicKey &public_key, Port port) {
    Enr enr{Secp256k1Signature{}, 1, public_key, Ip{1, 0, 0, 127}, port};
    rlp::Encoder rlp;
    rlp.bytes(enr.signature);
    rlp.uint(enr.sequence);
    rlp.str("id");
    rlp.str("v4");
    rlp.str("ip");
    rlp.bytes(enr.ip.value());
    rlp.str("secp256k1");
    rlp.bytes(enr.public_key);
    rlp.str("udp");
    rlp.uint(enr.port.value());
    return "enr:" + cppcodec::base64_url_unpadded::encode(rlp.list());
  }
}  // namespace lean::enr
