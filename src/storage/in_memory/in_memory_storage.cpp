/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/in_memory/in_memory_storage.hpp"

#include "storage/in_memory/cursor.hpp"
#include "storage/in_memory/in_memory_batch.hpp"
#include "storage/storage_error.hpp"

using qtils::ByteVec;

namespace lean::storage {

  outcome::result<ByteVecOrView> InMemoryStorage::get(
      const ByteView &key) const {
    if (storage_.find(key.toHex()) != storage_.end()) {
      return ByteView{storage_.at(key.toHex())};
    }

    return StorageError::NOT_FOUND;
  }

  outcome::result<std::optional<ByteVecOrView>> InMemoryStorage::tryGet(
      const qtils::ByteView &key) const {
    if (storage_.find(key.toHex()) != storage_.end()) {
      return ByteView{storage_.at(key.toHex())};
    }

    return std::nullopt;
  }

  outcome::result<void> InMemoryStorage::put(const ByteView &key,
                                             ByteVecOrView &&value) {
    auto it = storage_.find(key.toHex());
    if (it != storage_.end()) {
      size_t old_value_size = it->second.size();
      BOOST_ASSERT(size_ >= old_value_size);
      size_ -= old_value_size;
    }
    size_ += value.size();
    storage_[key.toHex()] = std::move(value).intoByteVec();
    return outcome::success();
  }

  outcome::result<bool> InMemoryStorage::contains(const ByteView &key) const {
    return storage_.find(key.toHex()) != storage_.end();
  }

  outcome::result<void> InMemoryStorage::remove(const ByteView &key) {
    auto it = storage_.find(key.toHex());
    if (it != storage_.end()) {
      size_ -= it->second.size();
      storage_.erase(it);
    }
    return outcome::success();
  }

  std::unique_ptr<BufferBatch> InMemoryStorage::batch() {
    return std::make_unique<InMemoryBatch>(*this);
  }

  std::unique_ptr<InMemoryStorage::Cursor> InMemoryStorage::cursor() {
    return std::make_unique<InMemoryCursor>(*this);
  }

  std::optional<size_t> InMemoryStorage::byteSizeHint() const {
    return size_;
  }
}  // namespace lean::storage
