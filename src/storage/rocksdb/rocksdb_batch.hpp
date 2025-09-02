/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <rocksdb/write_batch.h>

#include "storage/rocksdb/rocksdb.hpp"

namespace lean::storage {

  class RocksDbBatch : public BufferBatch {
   public:
    ~RocksDbBatch() override = default;

    RocksDbBatch(RocksDbSpace &db, log::Logger &logger);

    outcome::result<void> commit() override;

    void clear() override;

    outcome::result<void> put(const ByteView &key,
                              ByteVecOrView &&value) override;

    outcome::result<void> remove(const ByteView &key) override;

   private:
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    RocksDbSpace &db_;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    log::Logger &logger_;
    rocksdb::WriteBatch batch_;
  };
}  // namespace lean::storage
