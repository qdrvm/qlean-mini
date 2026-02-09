/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>

#include <boost/container/flat_map.hpp>
#include <qtils/shared_ref.hpp>
#include <rocksdb/db.h>
#include <rocksdb/table.h>
#include <rocksdb/utilities/db_ttl.h>

#include "log/logger.hpp"
#include "storage/buffer_map_types.hpp"
#include "storage/spaced_storage.hpp"
#include "utils/ctor_limiters.hpp"

namespace lean::app {
  class Configuration;
}

namespace lean::storage {

  class RocksDb : public SpacedStorage,
                  public std::enable_shared_from_this<RocksDb>,
                  NonCopyable,
                  NonMovable {
    using ColumnFamilyHandlePtr = rocksdb::ColumnFamilyHandle *;

   public:
    RocksDb(qtils::SharedRef<log::LoggingSystem> logsys,
            qtils::SharedRef<app::Configuration> app_config);

    ~RocksDb() override;

    static constexpr uint32_t kDefaultStateCacheSizeMiB = 512;
    static constexpr uint32_t kDefaultLruCacheSizeMiB = 512;
    static constexpr uint32_t kDefaultBlockSizeKiB = 32;

    std::shared_ptr<BufferStorage> getSpace(Space space) override;

    /**
     * Implementation-specific way to erase the whole space data.
     * Not exposed at SpacedStorage level as only used in pruner.
     * @param space - storage space identifier to clear
     */
    void dropColumn(Space space);

    /**
     * Prepare configuration structure
     * @param lru_cache_size_mib - LRU rocksdb cache in MiB
     * @param block_size_kib - internal rocksdb block size in KiB
     * @return options structure
     */
    static rocksdb::BlockBasedTableOptions tableOptionsConfiguration(
        uint32_t lru_cache_size_mib = kDefaultLruCacheSizeMiB,
        uint32_t block_size_kib = kDefaultBlockSizeKiB);

    friend class RocksDbSpace;
    friend class RocksDbBatch;

   private:
    struct DatabaseGuard {
      DatabaseGuard(
          std::shared_ptr<rocksdb::DB> db,
          std::vector<rocksdb::ColumnFamilyHandle *> column_family_handles,
          log::Logger log);

      DatabaseGuard(
          std::shared_ptr<rocksdb::DBWithTTL> db_ttl,
          std::vector<rocksdb::ColumnFamilyHandle *> column_family_handles,
          log::Logger log);

      ~DatabaseGuard();

     private:
      std::shared_ptr<rocksdb::DB> db_;
      std::shared_ptr<rocksdb::DBWithTTL> db_ttl_;
      std::vector<rocksdb::ColumnFamilyHandle *> column_family_handles_;
      log::Logger log_;
    };

    static outcome::result<void> createDirectory(
        const std::filesystem::path &absolute_path, log::Logger &log);

    static outcome::result<void> openDatabase(
        const rocksdb::Options &options,
        const std::filesystem::path &path,
        const std::vector<rocksdb::ColumnFamilyDescriptor>
            &column_family_descriptors,
        RocksDb &rocks_db,
        log::Logger &log);

    rocksdb::DB *db_{};
    std::vector<ColumnFamilyHandlePtr> column_family_handles_;
    boost::container::flat_map<Space, std::shared_ptr<BufferStorage>> spaces_;
    rocksdb::ReadOptions ro_;
    rocksdb::WriteOptions wo_;
    log::Logger logger_;
  };

  class RocksDbSpace : public BufferStorage {
   public:
    ~RocksDbSpace() override = default;

    RocksDbSpace(std::weak_ptr<RocksDb> storage,
                 const RocksDb::ColumnFamilyHandlePtr &column,
                 log::Logger logger);

    std::unique_ptr<BufferBatch> batch() override;

    std::optional<size_t> byteSizeHint() const override;

    std::unique_ptr<Cursor> cursor() override;

    outcome::result<bool> contains(const ByteView &key) const override;

    outcome::result<ByteVecOrView> get(const ByteView &key) const override;

    outcome::result<std::optional<ByteVecOrView>> tryGet(
        const ByteView &key) const override;

    outcome::result<void> put(const ByteView &key,
                              ByteVecOrView &&value) override;

    outcome::result<void> remove(const ByteView &key) override;

    void compact(const ByteVec &first, const ByteVec &last);

    friend class RocksDbBatch;

   private:
    // gather storage instance from weak ptr
    outcome::result<std::shared_ptr<RocksDb>> use() const;

    log::Logger logger_;
    std::weak_ptr<RocksDb> storage_;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const RocksDb::ColumnFamilyHandlePtr &column_;
  };
}  // namespace lean::storage
