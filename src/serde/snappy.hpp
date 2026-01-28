/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <snappy.h>

#include <boost/endian/conversion.hpp>
#include <crc32c/crc32c.h>
#include <libp2p/basic/read_varint.hpp>
#include <libp2p/basic/write_varint.hpp>
#include <libp2p/common/saturating.hpp>
#include <libp2p/connection/stream.hpp>
#include <qtils/bytes.hpp>
#include <qtils/bytestr.hpp>

namespace lean::snappy {
  enum class SnappyError {
    UNCOMPRESS_TOO_LONG,
    UNCOMPRESS_INVALID,
    UNCOMPRESS_TRUNCATED,
    UNCOMPRESS_UNKNOWN_IDENTIFIER,
    UNCOMPRESS_UNKNOWN_TYPE,
    UNCOMPRESS_CRC_MISMATCH,
  };
  Q_ENUM_ERROR_CODE(SnappyError) {
    using E = decltype(e);
    switch (e) {
      case E::UNCOMPRESS_TOO_LONG:
        return "SnappyError::UNCOMPRESS_TOO_LONG";
      case E::UNCOMPRESS_INVALID:
        return "SnappyError::UNCOMPRESS_INVALID";
      case E::UNCOMPRESS_TRUNCATED:
        return "SnappyError::UNCOMPRESS_TRUNCATED";
      case E::UNCOMPRESS_UNKNOWN_IDENTIFIER:
        return "SnappyError::UNCOMPRESS_UNKNOWN_IDENTIFIER";
      case E::UNCOMPRESS_UNKNOWN_TYPE:
        return "SnappyError::UNCOMPRESS_UNKNOWN_TYPE";
      case E::UNCOMPRESS_CRC_MISMATCH:
        return "SnappyError::UNCOMPRESS_CRC_MISMATCH";
    }
    abort();
  }

  constexpr size_t kHeaderSize = 4;
  constexpr auto kMaxBlockSize = size_t{1} << 16;
  constexpr auto kDefaultMaxSize = size_t{4} << 20;

  constexpr qtils::ByteArr<6> kStreamIdentifier{'s', 'N', 'a', 'P', 'p', 'Y'};

  enum ChunkType : uint8_t {
    Stream = 0xFF,
    Compressed = 0x00,
    Uncompressed = 0x01,
    Padding = 0xFE,
  };

  inline qtils::ByteVec compress(qtils::BytesIn input) {
    std::string compressed;
    ::snappy::Compress(
        qtils::byte2str(input.data()), input.size(), &compressed);
    return qtils::ByteVec{qtils::str2byte(std::as_const(compressed))};
  }

  inline outcome::result<qtils::ByteVec> uncompress(
      qtils::BytesIn compressed, size_t max_size = kDefaultMaxSize) {
    size_t size = 0;
    if (not ::snappy::GetUncompressedLength(
            qtils::byte2str(compressed.data()), compressed.size(), &size)) {
      return SnappyError::UNCOMPRESS_INVALID;
    }
    if (size > max_size) {
      return SnappyError::UNCOMPRESS_TOO_LONG;
    }
    std::string uncompressed;
    if (not ::snappy::Uncompress(qtils::byte2str(compressed.data()),
                                 compressed.size(),
                                 &uncompressed)) {
      return SnappyError::UNCOMPRESS_INVALID;
    }
    return qtils::ByteVec{qtils::str2byte(std::as_const(uncompressed))};
  }

  using Crc32 = qtils::ByteArr<4>;
  inline Crc32 hashCrc32(qtils::BytesIn input) {
    auto v = crc32c::Crc32c(input.data(), input.size());
    v = ((v >> 15) | (v << 17)) + 0xa282ead8;
    Crc32 crc;
    boost::endian::store_little_u32(crc.data(), v);
    return crc;
  }

  inline qtils::ByteVec compressFramed(qtils::BytesIn input) {
    qtils::ByteVec framed;
    auto write_header = [&](ChunkType type, size_t size) {
      framed.putUint8(type);
      qtils::ByteArr<3> size_bytes;
      boost::endian::store_little_u24(size_bytes.data(), size);
      framed.put(size_bytes);
    };
    write_header(ChunkType::Stream, kStreamIdentifier.size());
    framed.put(kStreamIdentifier);
    while (not input.empty()) {
      auto chunk = input.first(std::min(input.size(), kMaxBlockSize));
      auto crc = hashCrc32(chunk);
      input = input.subspan(chunk.size());
      auto compressed = compress(chunk);
      write_header(ChunkType::Compressed, Crc32::size() + compressed.size());
      framed.put(crc);
      framed.put(compressed);
    }
    return framed;
  }

  inline std::optional<size_t> chunkNeedBytes(qtils::BytesIn input) {
    if (input.size() < kHeaderSize) {
      return std::nullopt;
    }
    return kHeaderSize + boost::endian::load_little_u24(input.data() + 1);
  }

  inline outcome::result<qtils::ByteVec> uncompressFramed(
      qtils::BytesIn compressed, size_t max_size = kDefaultMaxSize) {
    qtils::ByteVec result;
    while (not compressed.empty()) {
      auto need = chunkNeedBytes(compressed);
      if (not need or compressed.size() < *need) {
        return SnappyError::UNCOMPRESS_TRUNCATED;
      }
      auto type = ChunkType{compressed[0]};
      auto size = boost::endian::load_little_u24(compressed.data() + 1);
      auto content = compressed.subspan(kHeaderSize, size);
      compressed = compressed.subspan(kHeaderSize + size);
      if (type == ChunkType::Stream) {
        if (qtils::ByteView{content} != kStreamIdentifier) {
          return SnappyError::UNCOMPRESS_UNKNOWN_IDENTIFIER;
        }
      } else if (type == ChunkType::Compressed
                 or type == ChunkType::Uncompressed) {
        qtils::ByteVec buffer;
        auto expected_crc = content.first(Crc32::size());
        auto uncompressed = content.subspan(Crc32::size());
        if (type == ChunkType::Compressed) {
          BOOST_OUTCOME_TRY(
              buffer,
              uncompress(uncompressed,
                         libp2p::saturating_sub(max_size, result.size())));
          uncompressed = buffer;
        }
        auto actual_crc = hashCrc32(uncompressed);
        if (qtils::ByteView{actual_crc} != expected_crc) {
          return SnappyError::UNCOMPRESS_CRC_MISMATCH;
        }
        result.put(uncompressed);
      } else if (type == ChunkType::Padding) {
        // skip padding
      } else {
        return SnappyError::UNCOMPRESS_UNKNOWN_TYPE;
      }
    }
    return result;
  }

  inline libp2p::CoroOutcome<qtils::ByteVec> coUncompressFramed(
      std::shared_ptr<libp2p::Stream> stream,
      size_t max_size = kDefaultMaxSize) {
    BOOST_OUTCOME_CO_TRY(auto size, co_await libp2p::readVarint(stream));
    if (size > max_size) {
      co_return SnappyError::UNCOMPRESS_TOO_LONG;
    }
    qtils::ByteVec result;
    while (result.size() < size) {
      qtils::ByteVec chunk;
      chunk.resize(kHeaderSize);
      BOOST_OUTCOME_CO_TRY(co_await libp2p::read(stream, chunk));
      auto need = chunkNeedBytes(chunk).value();
      chunk.resize(need);
      BOOST_OUTCOME_CO_TRY(
          co_await libp2p::read(stream, std::span{chunk}.subspan(kHeaderSize)));
      BOOST_OUTCOME_CO_TRY(
          auto uncompressed,
          uncompressFramed(chunk, libp2p::saturating_sub(size, result.size())));
      result.put(uncompressed);
    }
    co_return result;
  }

  inline libp2p::CoroOutcome<void> coCompressFramed(
      std::shared_ptr<libp2p::Stream> stream, qtils::BytesIn message) {
    auto compressed = compressFramed(message);
    BOOST_OUTCOME_CO_TRY(
        co_await libp2p::write(stream, libp2p::EncodeVarint{message.size()}));
    BOOST_OUTCOME_CO_TRY(co_await libp2p::write(stream, compressed));
    co_return outcome::success();
  }
}  // namespace lean::snappy
