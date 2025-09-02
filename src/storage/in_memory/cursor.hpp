/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/byte_vec.hpp>

#include "storage/in_memory/in_memory_storage.hpp"

namespace lean::storage {
  class InMemoryCursor : public BufferStorageCursor {
   public:
    explicit InMemoryCursor(InMemoryStorage &db) : db{db} {}

    outcome::result<bool> seekFirst() override {
      return seek(db.storage_.begin());
    }

    outcome::result<bool> seek(const ByteView &key) override {
      return seek(db.storage_.lower_bound(key.toHex()));
    }

    outcome::result<bool> seekLast() override {
      return seek(db.storage_.empty() ? db.storage_.end()
                                      : std::prev(db.storage_.end()));
    }

    bool isValid() const override {
      return kv.has_value();
    }

    outcome::result<void> next() override {
      seek(db.storage_.upper_bound(kv->first.toHex()));
      return outcome::success();
    }

    outcome::result<void> prev() override {
      auto it = db.storage_.lower_bound(kv->first.toHex());
      seek(it == db.storage_.begin() ? db.storage_.end() : std::prev(it));
      return outcome::success();
    }

    std::optional<ByteVec> key() const override {
      if (kv) {
        return kv->first;
      }
      return std::nullopt;
    }

    std::optional<ByteVecOrView> value() const override {
      if (kv) {
        return ByteView{kv->second};
      }
      return std::nullopt;
    }

   private:
    bool seek(decltype(InMemoryStorage::storage_)::iterator it) {
      if (it == db.storage_.end()) {
        kv.reset();
      } else {
        kv.emplace(ByteVec::fromHex(it->first).value(), it->second);
      }
      return isValid();
    }

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    InMemoryStorage &db;
    std::optional<std::pair<ByteVec, ByteVec>> kv;
  };
}  // namespace lean::storage
