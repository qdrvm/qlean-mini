/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include <qtils/byte_vec.hpp>
#include <qtils/outcome.hpp>

#include "storage/buffer_map_types.hpp"

namespace lean::storage {

  /**
   * Simple storage that conforms PersistentMap interface
   * Mostly needed to have an in-memory trie in tests to avoid integration with
   * an actual persistent database
   */
  class InMemoryStorage : public BufferStorage {
   public:
    ~InMemoryStorage() override = default;

    [[nodiscard]] outcome::result<ByteVecOrView> get(
        const ByteView &key) const override;

    [[nodiscard]] outcome::result<std::optional<ByteVecOrView>> tryGet(
        const ByteView &key) const override;

    outcome::result<void> put(const ByteView &key,
                              ByteVecOrView &&value) override;

    [[nodiscard]] outcome::result<bool> contains(
        const ByteView &key) const override;

    outcome::result<void> remove(const ByteView &key) override;

    std::unique_ptr<BufferBatch> batch() override;

    std::unique_ptr<Cursor> cursor() override;

    [[nodiscard]] std::optional<size_t> byteSizeHint() const override;

   private:
    std::map<std::string, ByteVec> storage_;
    size_t size_ = 0;

    friend class InMemoryCursor;
  };

}  // namespace lean::storage
