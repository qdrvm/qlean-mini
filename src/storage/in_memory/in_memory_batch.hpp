/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/byte_vec.hpp>
#include "storage/in_memory/in_memory_storage.hpp"

namespace lean::storage {
  using qtils::ByteVec;

  class InMemoryBatch : public BufferBatch {
   public:
    explicit InMemoryBatch(InMemoryStorage &db) : db{db} {}

    outcome::result<void> put(const ByteView &key,
                              ByteVecOrView &&value) override {
      entries[key.toHex()] = std::move(value).intoByteVec();
      return outcome::success();
    }

    outcome::result<void> remove(const ByteView &key) override {
      entries.erase(key.toHex());
      return outcome::success();
    }

    outcome::result<void> commit() override {
      for (auto &entry : entries) {
        OUTCOME_TRY(db.put(ByteVec::fromHex(entry.first).value(),
                           ByteView{entry.second}));
      }
      return outcome::success();
    }

    void clear() override {
      entries.clear();
    }

   private:
    std::map<std::string, ByteVec> entries;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    InMemoryStorage &db;
  };
}  // namespace lean::storage
