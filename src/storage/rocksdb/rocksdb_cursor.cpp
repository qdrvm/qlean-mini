/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/rocksdb/rocksdb_cursor.hpp"

#include "rocksdb_util.hpp"

namespace lean::storage {

  RocksDBCursor::RocksDBCursor(std::shared_ptr<rocksdb::Iterator> it)
      : i_{std::move(it)} {}

  outcome::result<bool> RocksDBCursor::seekFirst() {
    i_->SeekToFirst();
    return isValid();
  }

  outcome::result<bool> RocksDBCursor::seek(const ByteView &key) {
    i_->Seek(make_slice(key));
    return isValid();
  }

  outcome::result<bool> RocksDBCursor::seekLast() {
    i_->SeekToLast();
    return isValid();
  }

  bool RocksDBCursor::isValid() const {
    return i_->Valid();
  }

  outcome::result<void> RocksDBCursor::next() {
    i_->Next();
    return outcome::success();
  }

  outcome::result<void> RocksDBCursor::prev() {
    i_->Prev();
    return outcome::success();
  }

  std::optional<ByteVec> RocksDBCursor::key() const {
    return isValid() ? std::make_optional(make_buffer(i_->key()))
                     : std::nullopt;
  }

  std::optional<ByteVecOrView> RocksDBCursor::value() const {
    return isValid() ? std::make_optional(make_buffer(i_->value()))
                     : std::nullopt;
  }
}  // namespace lean::storage
