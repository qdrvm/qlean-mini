/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/byte_vec.hpp>
#include <rocksdb/status.h>

#include "storage/storage_error.hpp"

namespace lean::storage {
  inline StorageError status_as_error(const rocksdb::Status &s,
                                      const log::Logger &log) {
    if (s.IsNotFound()) {
      return StorageError::NOT_FOUND;
    }

    if (s.IsIOError()) {
      SL_ERROR(log, ":{}", s.ToString());
      return StorageError::IO_ERROR;
    }

    if (s.IsInvalidArgument()) {
      return StorageError::INVALID_ARGUMENT;
    }

    if (s.IsCorruption()) {
      return StorageError::CORRUPTION;
    }

    if (s.IsNotSupported()) {
      return StorageError::NOT_SUPPORTED;
    }

    return StorageError::UNKNOWN;
  }

  inline rocksdb::Slice make_slice(const qtils::ByteView &buf) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto *ptr = reinterpret_cast<const char *>(buf.data());
    size_t n = buf.size();
    return rocksdb::Slice{ptr, n};
  }

  inline ByteView make_span(const rocksdb::Slice &s) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return {reinterpret_cast<const uint8_t *>(s.data()), s.size()};
  }

  inline qtils::ByteVec make_buffer(const rocksdb::Slice &s) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto *ptr = reinterpret_cast<const uint8_t *>(s.data());
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return {ptr, ptr + s.size()};
  }
}  // namespace lean::storage
