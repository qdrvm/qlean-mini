/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <rocksdb/iterator.h>

#include "storage/rocksdb/rocksdb.hpp"

namespace lean::storage {

  class RocksDBCursor : public BufferStorageCursor {
   public:
    ~RocksDBCursor() override = default;

    explicit RocksDBCursor(std::shared_ptr<rocksdb::Iterator> it);

    outcome::result<bool> seekFirst() override;

    outcome::result<bool> seek(const ByteView &key) override;

    outcome::result<bool> seekLast() override;

    bool isValid() const override;

    outcome::result<void> next() override;

    outcome::result<void> prev() override;

    std::optional<ByteVec> key() const override;

    std::optional<ByteVecOrView> value() const override;

   private:
    std::shared_ptr<rocksdb::Iterator> i_;
  };

}  // namespace lean::storage
