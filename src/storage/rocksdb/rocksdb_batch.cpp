/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/rocksdb/rocksdb_batch.hpp"

#include "storage/rocksdb/rocksdb_util.hpp"
#include "storage/storage_error.hpp"

namespace lean::storage {

  RocksDbBatch::RocksDbBatch(RocksDbSpace &db, log::Logger &logger)
      : db_(db), logger_(logger) {}

  outcome::result<void> RocksDbBatch::put(const ByteView &key,
                                          ByteVecOrView &&value) {
    batch_.Put(db_.column_, make_slice(key), make_slice(std::move(value)));
    return outcome::success();
  }

  outcome::result<void> RocksDbBatch::remove(const ByteView &key) {
    batch_.Delete(db_.column_, make_slice(key));
    return outcome::success();
  }

  outcome::result<void> RocksDbBatch::commit() {
    auto rocks = db_.storage_.lock();
    if (!rocks) {
      return StorageError::STORAGE_GONE;
    }
    auto status = rocks->db_->Write(rocks->wo_, &batch_);
    if (status.ok()) {
      return outcome::success();
    }

    return status_as_error(status, logger_);
  }

  void RocksDbBatch::clear() {
    batch_.Clear();
  }
}  // namespace lean::storage
