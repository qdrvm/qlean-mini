/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "serde/enr.hpp"

#include <bit>

#include <boost/asio/ip/address_v4.hpp>
#include <boost/endian/conversion.hpp>
#include <cppcodec/base64_url_unpadded.hpp>
#include <libp2p/crypto/secp256k1_provider/secp256k1_provider_impl.hpp>
#include <libp2p/crypto/sha/keccak.hpp>
#include <libp2p/peer/peer_id_from.hpp>
#include <qtils/bytestr.hpp>

namespace lean::rlp {
  constexpr uint8_t kMaxPrefix1 = 55;
  constexpr uint8_t kBytesPrefix1 = 0x80;
  constexpr uint8_t kListPrefix1 = 0xc0;

  enum class Error {
    INVALID_RLP,
    INT_OVERFLOW,
    INVALID_KEY_VALUE,
    KEY_NOT_FOUND,
  };
  Q_ENUM_ERROR_CODE(Error) {
    using E = decltype(e);
    switch (e) {
      case E::INVALID_RLP:
        return "Invalid rlp";
      case E::INT_OVERFLOW:
        return "Int overflow";
      case E::INVALID_KEY_VALUE:
        return "Invalid key-value";
      case E::KEY_NOT_FOUND:
        return "Key not found";
    }
    abort();
  }

  struct Decoder {
    qtils::BytesIn input_;

    bool empty() const {
      return input_.empty();
    }

    outcome::result<qtils::BytesIn> take(size_t n) {
      if (n > input_.size()) {
        return Error::INVALID_RLP;
      }
      auto r = input_.first(n);
      input_ = input_.subspan(n);
      return r;
    }

    template <typename T>
    static outcome::result<T> uint(qtils::BytesIn be) {
      if (be.size() * 8 > std::numeric_limits<T>::digits) {
        return Error::INT_OVERFLOW;
      }
      T v = 0;
      for (auto &x : be) {
        v = (v << 8) | x;
      }
      return v;
    }

    template <uint8_t base1>
    outcome::result<qtils::BytesIn> bytesInternal() {
      constexpr auto base2 = base1 + kMaxPrefix1;
      if (base1 > input_[0]) {
        return Error::INVALID_RLP;
      }
      if (input_[0] <= base2) {
        auto n = input_[0] - base1;
        BOOST_OUTCOME_TRY(take(1));
        return take(n);
      }
      auto n1 = input_[0] - base2;
      BOOST_OUTCOME_TRY(take(1));
      BOOST_OUTCOME_TRY(auto n2_raw, take(n1));
      BOOST_OUTCOME_TRY(auto n2, uint<size_t>(n2_raw));
      return take(n2);
    }

    bool is_list() const {
      return not empty() and kListPrefix1 <= input_[0];
    }

    outcome::result<Decoder> list() {
      if (empty()) {
        return Error::INVALID_RLP;
      }
      BOOST_OUTCOME_TRY(auto raw, bytesInternal<kListPrefix1>());
      return Decoder{raw};
    }

    outcome::result<qtils::BytesIn> bytes() {
      if (empty()) {
        return Error::INVALID_RLP;
      }
      if (input_[0] >= kListPrefix1) {
        return Error::INVALID_RLP;
      }
      if (input_[0] < kBytesPrefix1) {
        return take(1);
      }
      return bytesInternal<kBytesPrefix1>();
    }

    template <typename T>
      requires std::is_default_constructible_v<T>
           and requires(T t) { qtils::BytesOut{t}; }
    outcome::result<T> bytes_n() {
      BOOST_OUTCOME_TRY(auto raw, bytes());
      T r;
      if (raw.size() != r.size()) {
        return Error::INVALID_RLP;
      }
      memcpy(r.data(), raw.data(), r.size());
      return r;
    }

    template <size_t N>
    outcome::result<qtils::ByteArr<N>> bytes_n() {
      return bytes_n<qtils::ByteArr<N>>();
    }

    outcome::result<std::string_view> str() {
      BOOST_OUTCOME_TRY(auto raw, bytes());
      return qtils::byte2str(raw);
    }

    template <typename T>
    outcome::result<T> uint() {
      BOOST_OUTCOME_TRY(auto be, bytes());
      return uint<T>(be);
    }

    outcome::result<bool> readBool() {
      BOOST_OUTCOME_TRY(auto byte, uint<uint8_t>());
      if (byte == 0) {
        return false;
      }
      if (byte == 1) {
        return true;
      }
      return Error::INT_OVERFLOW;
    }

    outcome::result<void> skip() {
      if (is_list()) {
        BOOST_OUTCOME_TRY(list());
      } else {
        BOOST_OUTCOME_TRY(bytes());
      }
      return outcome::success();
    }

    using KeyValue = std::unordered_map<std::string_view, Decoder>;
    outcome::result<KeyValue> keyValue() {
      KeyValue kv;
      while (not empty()) {
        BOOST_OUTCOME_TRY(auto key, str());
        if (empty()) {
          return Error::INVALID_KEY_VALUE;
        }
        auto value = input_;
        BOOST_OUTCOME_TRY(skip());
        value = value.first(value.size() - input_.size());
        kv.emplace(key, Decoder{value});
      }
      return kv;
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
    void uint(uint64_t v) {
      auto n = sizeof(uint64_t) - std::countl_zero(v) / 8;
      size_ = 1 + n;
      buffer_[0] = base + n;
      for (size_t i = 1; i < size_; ++i) {
        buffer_[i] = v >> (8 * (n - i));
      }
    }
    template <uint8_t base>
    void bytesInternal(size_t bytes) {
      if (bytes <= kMaxPrefix1) {
        size_ = 1;
        buffer_[0] = base + bytes;
      } else {
        uint<base + kMaxPrefix1>(bytes);
      }
    }
  };

  struct EncodeUint : EncodeBuffer {
    EncodeUint(uint64_t v) {
      if (0 < v and v < kBytesPrefix1) {
        size_ = 1;
        buffer_[0] = v;
      } else {
        uint<kBytesPrefix1>(v);
      }
    }
  };

  struct EncodeBytes : EncodeBuffer {
    EncodeBytes(qtils::BytesIn bytes) {
      if (bytes.size() == 1 and bytes[0] < kBytesPrefix1) {
        size_ = 1;
        buffer_[0] = bytes[0];
      } else {
        bytesInternal<kBytesPrefix1>(bytes.size());
      }
    }
  };

  struct EncodeList : EncodeBuffer {
    EncodeList(size_t bytes) {
      bytesInternal<kListPrefix1>(bytes);
    }
  };

  struct Encoder {
    qtils::ByteVec output_;

    void uint(uint64_t v) {
      output_.put(EncodeUint{v});
    }

    void writeBool(bool v) {
      uint(v ? 1 : 0);
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
  enum class Error {
    EXPECTED_SECP256K1_KEYPAIR,
    INVALID_PREFIX,
    INVALID_ID,
    SIGNATURE_VERIFICATION_FAILED,
  };
  Q_ENUM_ERROR_CODE(Error) {
    using E = decltype(e);
    switch (e) {
      case E::EXPECTED_SECP256K1_KEYPAIR:
        return "Expected secp256k1 keypair";
      case E::INVALID_PREFIX:
        return "Invalid ENR prefix";
      case E::INVALID_ID:
        return "Invalid ENR id";
      case E::SIGNATURE_VERIFICATION_FAILED:
        return "Signature verification failed";
    }
    abort();
  }

  Ip makeIp(uint32_t i) {
    Ip ip;
    boost::endian::store_big_u32(ip.data(), i);
    return ip;
  }

  Ip makeIp(std::string_view base, uint32_t i) {
    auto ip_host_endian = boost::asio::ip::make_address_v4(base).to_uint();
    return makeIp(ip_host_endian + i);
  }

  std::string toString(const Ip &ip) {
    auto ip_host_endian = boost::endian::load_big_u32(ip.data());
    auto ip_asio = boost::asio::ip::make_address_v4(ip_host_endian);
    return ip_asio.to_string();
  }

  libp2p::PeerId Enr::peerId() const {
    return libp2p::peerIdFromSecp256k1(public_key);
  }

  libp2p::Multiaddress Enr::listenAddress() const {
    return libp2p::Multiaddress::create(
               std::format("/ip4/0.0.0.0/udp/{}/quic-v1", port.value()))
        .value();
  }

  libp2p::Multiaddress Enr::connectAddress() const {
    auto &ip = this->ip.value();
    return libp2p::Multiaddress::create(
               std::format("/ip4/{}/udp/{}/quic-v1/p2p/{}",
                           toString(ip),
                           port.value(),
                           peerId().toBase58()))
        .value();
  }

  libp2p::PeerInfo Enr::connectInfo() const {
    return {peerId(), {connectAddress()}};
  }
  bool Enr::isAggregator() const {
    return is_aggregator_optional == true;
  }

  inline void encodeContent(rlp::Encoder &rlp, const Enr &enr) {
    rlp.uint(enr.sequence);
    rlp.str("id");
    rlp.str("v4");
    rlp.str("ip");
    rlp.bytes(enr.ip.value());
    if (enr.is_aggregator_optional.has_value()) {
      rlp.str("is_aggregator");
      rlp.writeBool(*enr.is_aggregator_optional);
    }
    rlp.str("quic");
    rlp.uint(enr.port.value());
    rlp.str("secp256k1");
    rlp.bytes(enr.public_key);
  }

  qtils::ByteVec Enr::signable() const {
    rlp::Encoder rlp;
    encodeContent(rlp, *this);
    return rlp.list();
  }

  outcome::result<Enr> decode(std::string_view str) {
    constexpr std::string_view s_enr{"enr:"};
    if (not str.starts_with(s_enr)) {
      return Error::INVALID_PREFIX;
    }
    str.remove_prefix(s_enr.size());
    auto rlp_bytes = cppcodec::base64_url_unpadded::decode(str);
    rlp::Decoder rlp{rlp_bytes};
    BOOST_OUTCOME_TRY(rlp, rlp.list());
    Enr enr;
    BOOST_OUTCOME_TRY(enr.signature, rlp.bytes_n<Secp256k1Signature>());
    BOOST_OUTCOME_TRY(enr.sequence, rlp.uint<Sequence>());
    BOOST_OUTCOME_TRY(auto kv, rlp.keyValue());

    auto kv_id = kv.find("id");
    if (kv_id == kv.end()) {
      return rlp::Error::KEY_NOT_FOUND;
    }
    BOOST_OUTCOME_TRY(auto id, kv_id->second.str());
    if (id != "v4") {
      return Error::INVALID_ID;
    }

    auto kv_ip = kv.find("ip");
    if (kv_ip != kv.end()) {
      BOOST_OUTCOME_TRY(enr.ip, kv_ip->second.bytes_n<Ip>());
    }

    auto kv_secp256k1 = kv.find("secp256k1");
    if (kv_secp256k1 == kv.end()) {
      return rlp::Error::KEY_NOT_FOUND;
    }
    BOOST_OUTCOME_TRY(enr.public_key,
                      kv_secp256k1->second.bytes_n<Secp256k1PublicKey>());

    auto kv_quic = kv.find("quic");
    if (kv_quic != kv.end()) {
      BOOST_OUTCOME_TRY(enr.port, kv_quic->second.uint<Port>());
    }

    auto kv_is_aggregator = kv.find("is_aggregator");
    if (kv_is_aggregator != kv.end()) {
      BOOST_OUTCOME_TRY(enr.is_aggregator_optional,
                        kv_is_aggregator->second.readBool());
    }

    libp2p::crypto::secp256k1::Secp256k1ProviderImpl secp256k1{nullptr};
    BOOST_OUTCOME_TRY(
        auto valid_signature,
        secp256k1.verifyCompact(libp2p::Keccak::hash(enr.signable()),
                                enr.signature,
                                enr.public_key));
    if (not valid_signature) {
      return Error::SIGNATURE_VERIFICATION_FAILED;
    }

    return enr;
  }

  outcome::result<std::string> encode(const libp2p::crypto::KeyPair &keypair,
                                      Ip ip,
                                      Port port,
                                      std::optional<bool> is_aggregator) {
    if (keypair.privateKey.type != libp2p::crypto::Key::Type::Secp256k1) {
      return Error::EXPECTED_SECP256K1_KEYPAIR;
    }
    if (keypair.publicKey.type != libp2p::crypto::Key::Type::Secp256k1) {
      return Error::EXPECTED_SECP256K1_KEYPAIR;
    }
    BOOST_OUTCOME_TRY(auto private_key,
                      Secp256k1PrivateKey::fromSpan(keypair.privateKey.data));
    BOOST_OUTCOME_TRY(auto public_key,
                      Secp256k1PublicKey::fromSpan(keypair.publicKey.data));

    Enr enr{
        .sequence = 1,
        .public_key = public_key,
        .ip = ip,
        .port = port,
        .is_aggregator_optional = is_aggregator,
    };

    libp2p::crypto::secp256k1::Secp256k1ProviderImpl secp256k1{nullptr};
    enr.signature = Secp256k1Signature{
        secp256k1.signCompact(libp2p::Keccak::hash(enr.signable()), private_key)
            .value()};

    rlp::Encoder rlp;
    rlp.bytes(enr.signature);
    encodeContent(rlp, enr);
    return "enr:" + cppcodec::base64_url_unpadded::encode(rlp.list());
  }
}  // namespace lean::enr
