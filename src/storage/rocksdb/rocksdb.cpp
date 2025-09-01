/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/rocksdb/rocksdb.hpp"

#include <algorithm>
#include <iterator>
#include <ranges>

#include <app/configuration.hpp>
#include <qtils/cxx23/ranges/contains.hpp>
#include <qtils/error_throw.hpp>
#include <rocksdb/filter_policy.h>
#include <rocksdb/table.h>
#include <soralog/macro.hpp>

#include "storage/rocksdb/rocksdb_batch.hpp"
#include "storage/rocksdb/rocksdb_cursor.hpp"
#include "storage/rocksdb/rocksdb_spaces.hpp"
#include "storage/rocksdb/rocksdb_util.hpp"
#include "storage/storage_error.hpp"
#include "utils/fd_limit.hpp"

namespace lean::storage {
  namespace fs = std::filesystem;

  rocksdb::ColumnFamilyOptions configureColumn(uint64_t memory_budget) {
    rocksdb::ColumnFamilyOptions options;
    options.OptimizeLevelStyleCompaction(memory_budget);
    auto table_options = RocksDb::tableOptionsConfiguration();
    options.table_factory.reset(NewBlockBasedTableFactory(table_options));
    return options;
  }

  template <std::ranges::range ColumnFamilyNames>
  void configureColumnFamilies(
      std::vector<rocksdb::ColumnFamilyDescriptor> &column_family_descriptors,
      std::vector<int32_t> &ttls,
      ColumnFamilyNames &&cf_names,
      const std::unordered_map<std::string, int32_t> &column_ttl,
      const std::unordered_map<std::string, double> &column_cache_sizes,
      uint64_t memory_budget,
      log::Logger &log) {
    double distributed_cache_part = 0;
    size_t count = 0;
    for (const auto &[column, value] : column_cache_sizes) {
      if (qtils::cxx23::ranges::contains(cf_names, column)) {
        distributed_cache_part += value;
        ++count;
      }
    }
    BOOST_ASSERT_MSG(distributed_cache_part <= 1.0,
                     "Special cache distribution must not be greater 100%");

    const uint64_t other_spaces_cache_size =
        (cf_names.size() > count)
            ? static_cast<double>(memory_budget)
                  * (1.0 - distributed_cache_part) / (cf_names.size() - count)
            : 0;

    for (auto &space_name : std::forward<ColumnFamilyNames>(cf_names)) {
      auto ttl = 0;
      auto cache_size = 0ull;
      if (const auto it = column_ttl.find(space_name); it != column_ttl.end()) {
        ttl = it->second;
      }
      if (const auto it = column_cache_sizes.find(space_name);
          it != column_cache_sizes.end()) {
        cache_size = static_cast<double>(memory_budget) * it->second;
      } else {
        cache_size = other_spaces_cache_size;
      }
      auto column_options = configureColumn(cache_size);
      column_family_descriptors.emplace_back(space_name, column_options);
      ttls.push_back(ttl);
      SL_DEBUG(
          log,
          "Column family '{}' configured with ttl={}sec, cache_size={:.0f}Mb",
          space_name,
          ttl,
          static_cast<double>(cache_size) / 1024.0 / 1024.0);
    }
  }

  RocksDb::RocksDb(qtils::SharedRef<log::LoggingSystem> logsys,
                   qtils::SharedRef<app::Configuration> app_config)
      : logger_(logsys->getLogger("RocksDB", "storage")) {
    ro_.fill_cache = false;

    const auto &path = app_config->database().directory;

    auto options = rocksdb::Options{};
    options.create_if_missing = true;
    options.optimize_filters_for_hits = true;
    options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(
        storage::RocksDb::tableOptionsConfiguration()));

    // Setting limit for open rocksdb files to a half of system soft limit
    auto soft_limit = getFdLimit(logger_);
    if (!soft_limit) {
      SL_CRITICAL(logger_, "Call getrlimit(RLIMIT_NOFILE) was failed");
      qtils::raise(StorageError::UNKNOWN);
    }
    // NOLINTNEXTLINE(cppcoreguidelines-narrowing-conversions)
    options.max_open_files = soft_limit.value() / 2;

    const auto no_db_presented = not exists(path);

    std::error_code ec;
    create_directories(path, ec);
    if (ec) {
      SL_CRITICAL(logger_, "Can't create DB directory: {}", ec);
      qtils::raise(ec);
    }

    if (auto res = createDirectory(path, logger_); res.has_error()) {
      SL_CRITICAL(logger_,
                  "Can't create DB directory ({}): {}",
                  path.native(),
                  res.error());
      qtils::raise(ec);
    }

    std::vector<std::string> existing_families;
    auto res = rocksdb::DB::ListColumnFamilies(
        options, path.native(), &existing_families);
    if (not res.ok() and not res.IsPathNotFound()) {
      SL_ERROR(logger_,
               "Can't list column families in {}: {}",
               path.native(),
               res.ToString());
      qtils::raise(status_as_error(res, logger_));
    }

    std::unordered_set<std::string> all_families;
    auto required_families =
        std::views::iota(0, static_cast<int>(SpacesCount))
        | std::views::transform([](int i) -> std::string {
            return std::string(spaceName(static_cast<Space>(i)));
          });
    std::ranges::copy(required_families,
                      std::inserter(all_families, all_families.end()));

    for (auto &existing_family : existing_families) {
      auto [_, was_inserted] = all_families.insert(existing_family);
      if (was_inserted) {
        SL_WARN(logger_,
                "Column family '{}' present in database but not used by Lean; "
                "Probably obsolete.",
                existing_family);
      }
    }

    const std::unordered_map<std::string, int32_t> column_ttl = {
        // Example:
        // {"avaliability_storage", 25 * 60 * 60}  // 25 hours
    };

    const std::unordered_map<std::string, double> column_cache_size = {
        // Example:
        // {"trie_node", 0.9}  // 90%
    };

    const auto memory_budget = app_config->database().cache_size;

    std::vector<rocksdb::ColumnFamilyDescriptor> column_family_descriptors;
    std::vector<int32_t> ttls;
    configureColumnFamilies(column_family_descriptors,
                            ttls,
                            all_families,
                            column_ttl,
                            column_cache_size,
                            memory_budget,
                            logger_);

    options.create_missing_column_families = true;

    if (no_db_presented) {
      qtils::raise_on_err(openDatabaseWithTTL(options,
                                              path,
                                              column_family_descriptors,
                                              ttls,
                                              *this,
                                              logger_));
    }

    // Print size of each column family
    SL_VERBOSE(logger_, "Current column family sizes:");
    for (const auto &handle : column_family_handles_) {
      std::string size_str;
      if (db_->GetProperty(
              handle, "rocksdb.estimate-live-data-size", &size_str)) {
        uint64_t size_bytes = std::stoull(size_str);
        double size_mb = static_cast<double>(size_bytes) / 1024.0 / 1024.0;
        SL_VERBOSE(logger_, "  - {}: {:.2f} Mb", handle->GetName(), size_mb);
      } else {
        SL_WARN(logger_,
                "Failed to get size of column family '{}'",
                handle->GetName());
      }
    }
  }

  RocksDb::~RocksDb() {
    for (auto *handle : column_family_handles_) {
      db_->DestroyColumnFamilyHandle(handle);
    }
    delete db_;
  }

  outcome::result<void> RocksDb::createDirectory(
      const std::filesystem::path &absolute_path, log::Logger &log) {
    std::error_code ec;
    if (not fs::create_directory(absolute_path.native(), ec) and ec.value()) {
      SL_ERROR(log,
               "Can't create directory {} for database: {}",
               absolute_path.native(),
               ec);
      return StorageError::IO_ERROR;
    }
    if (not fs::is_directory(absolute_path.native())) {
      SL_ERROR(log,
               "Can't open {} for database: is not a directory",
               absolute_path.native());
      return StorageError::IO_ERROR;
    }
    return outcome::success();
  }

  outcome::result<void> RocksDb::openDatabaseWithTTL(
      const rocksdb::Options &options,
      const std::filesystem::path &path,
      const std::vector<rocksdb::ColumnFamilyDescriptor>
          &column_family_descriptors,
      const std::vector<int32_t> &ttls,
      RocksDb &rocks_db,
      log::Logger &log) {
    const auto status =
        rocksdb::DBWithTTL::Open(options,
                                 path.native(),
                                 column_family_descriptors,
                                 &rocks_db.column_family_handles_,
                                 &rocks_db.db_,
                                 ttls);
    if (not status.ok()) {
      SL_ERROR(log,
               "Can't open database in {}: {}",
               path.native(),
               status.ToString());
      return status_as_error(status, log);
    }
    return outcome::success();
  }

  std::shared_ptr<BufferStorage> RocksDb::getSpace(Space space) {
    if (spaces_.contains(space)) {
      return spaces_[space];
    }
    auto space_name = spaceName(space);
    auto column = std::ranges::find_if(
        column_family_handles_,
        [&space_name](const ColumnFamilyHandlePtr &handle) {
          return handle->GetName() == space_name;
        });
    if (column_family_handles_.end() == column) {
      throw StorageError::INVALID_ARGUMENT;
    }
    auto space_ptr =
        std::make_shared<RocksDbSpace>(weak_from_this(), *column, logger_);
    spaces_[space] = space_ptr;
    return space_ptr;
  }

  void RocksDb::dropColumn(lean::storage::Space space) {
    auto space_name = spaceName(space);
    auto column_it = std::ranges::find_if(
        column_family_handles_,
        [&space_name](const ColumnFamilyHandlePtr &handle) {
          return handle->GetName() == space_name;
        });
    if (column_family_handles_.end() == column_it) {
      throw StorageError::INVALID_ARGUMENT;
    }
    auto &handle = *column_it;
    auto e = [this](const rocksdb::Status &status) {
      if (!status.ok()) {
        logger_->error("DB operation failed: {}", status.ToString());
        throw status_as_error(status, logger_);
      }
    };
    e(db_->DropColumnFamily(handle));
    e(db_->DestroyColumnFamilyHandle(handle));
    e(db_->CreateColumnFamily({}, std::string(space_name), &handle));
  }

  rocksdb::BlockBasedTableOptions RocksDb::tableOptionsConfiguration(
      uint32_t lru_cache_size_mib, uint32_t block_size_kib) {
    rocksdb::BlockBasedTableOptions table_options;
    table_options.format_version = 5;
    table_options.block_cache = rocksdb::NewLRUCache(
        static_cast<uint64_t>(lru_cache_size_mib * 1024 * 1024));
    table_options.block_size = static_cast<size_t>(block_size_kib * 1024);
    table_options.cache_index_and_filter_blocks = true;
    table_options.filter_policy.reset(rocksdb::NewBloomFilterPolicy(10, false));
    return table_options;
  }

  RocksDb::DatabaseGuard::DatabaseGuard(
      std::shared_ptr<rocksdb::DB> db,
      std::vector<rocksdb::ColumnFamilyHandle *> column_family_handles,
      log::Logger log)
      : db_(std::move(db)),
        column_family_handles_(std::move(column_family_handles)),
        log_(std::move(log)) {}

  RocksDb::DatabaseGuard::DatabaseGuard(
      std::shared_ptr<rocksdb::DBWithTTL> db_ttl,
      std::vector<rocksdb::ColumnFamilyHandle *> column_family_handles,
      log::Logger log)
      : db_ttl_(std::move(db_ttl)),
        column_family_handles_(std::move(column_family_handles)),
        log_(std::move(log)) {}

  RocksDb::DatabaseGuard::~DatabaseGuard() {
    const auto clean = [this](auto db) {
      auto status = db->Flush(rocksdb::FlushOptions());
      if (not status.ok()) {
        SL_ERROR(log_, "Can't flush database: {}", status.ToString());
      }

      status = db->WaitForCompact(rocksdb::WaitForCompactOptions());
      if (not status.ok()) {
        SL_ERROR(log_,
                 "Can't wait for background compaction: {}",
                 status.ToString());
      }

      for (auto *handle : column_family_handles_) {
        db->DestroyColumnFamilyHandle(handle);
      }

      status = db->Close();
      if (not status.ok()) {
        SL_ERROR(log_, "Can't close database: {}", status.ToString());
      }
      db.reset();
    };
    if (db_) {
      clean(db_);
    } else if (db_ttl_) {
      clean(db_ttl_);
    }
  }

  RocksDbSpace::RocksDbSpace(std::weak_ptr<RocksDb> storage,
                             const RocksDb::ColumnFamilyHandlePtr &column,
                             log::Logger logger)
      : storage_{std::move(storage)},
        column_{column},
        logger_{std::move(logger)} {}

  std::unique_ptr<BufferBatch> RocksDbSpace::batch() {
    return std::make_unique<RocksDbBatch>(*this, logger_);
  }

  std::optional<size_t> RocksDbSpace::byteSizeHint() const {
    auto rocks = storage_.lock();
    if (!rocks) {
      return 0;
    }
    size_t usage_bytes = 0;
    if (rocks->db_) {
      std::string usage;
      bool result =
          rocks->db_->GetProperty("rocksdb.cur-size-all-mem-tables", &usage);
      if (result) {
        try {
          usage_bytes = std::stoul(usage);
        } catch (...) {
          logger_->error("Unable to parse memory usage value");
        }
      } else {
        logger_->error("Unable to retrieve memory usage value");
      }
    }
    return usage_bytes;
  }

  std::unique_ptr<RocksDbSpace::Cursor> RocksDbSpace::cursor() {
    auto rocks = storage_.lock();
    if (!rocks) {
      throw StorageError::STORAGE_GONE;
    }
    auto it = std::unique_ptr<rocksdb::Iterator>(
        rocks->db_->NewIterator(rocks->ro_, column_));
    return std::make_unique<RocksDBCursor>(std::move(it));
  }

  outcome::result<bool> RocksDbSpace::contains(const ByteView &key) const {
    OUTCOME_TRY(rocks, use());
    std::string value;
    auto status = rocks->db_->Get(rocks->ro_, column_, make_slice(key), &value);
    if (status.ok()) {
      return true;
    }

    if (status.IsNotFound()) {
      return false;
    }

    return status_as_error(status, logger_);
  }

  outcome::result<ByteVecOrView> RocksDbSpace::get(const ByteView &key) const {
    OUTCOME_TRY(rocks, use());
    std::string value;
    auto status = rocks->db_->Get(rocks->ro_, column_, make_slice(key), &value);
    if (status.ok()) {
      // cannot move string content to a buffer
      return ByteVec(
          reinterpret_cast<uint8_t *>(value.data()),                  // NOLINT
          reinterpret_cast<uint8_t *>(value.data()) + value.size());  // NOLINT
    }
    return status_as_error(status, logger_);
  }

  outcome::result<std::optional<ByteVecOrView>> RocksDbSpace::tryGet(
      const ByteView &key) const {
    OUTCOME_TRY(rocks, use());
    std::string value;
    auto status = rocks->db_->Get(rocks->ro_, column_, make_slice(key), &value);
    if (status.ok()) {
      auto buf = ByteVec(
          reinterpret_cast<uint8_t *>(value.data()),                  // NOLINT
          reinterpret_cast<uint8_t *>(value.data()) + value.size());  // NOLINT
      return std::make_optional(ByteVecOrView(std::move(buf)));
    }

    if (status.IsNotFound()) {
      return std::nullopt;
    }

    return status_as_error(status, logger_);
  }

  outcome::result<void> RocksDbSpace::put(const ByteView &key,
                                          ByteVecOrView &&value) {
    OUTCOME_TRY(rocks, use());
    auto status = rocks->db_->Put(
        rocks->wo_, column_, make_slice(key), make_slice(std::move(value)));
    if (status.ok()) {
      return outcome::success();
    }

    return status_as_error(status, logger_);
  }

  outcome::result<void> RocksDbSpace::remove(const ByteView &key) {
    OUTCOME_TRY(rocks, use());
    auto status = rocks->db_->Delete(rocks->wo_, column_, make_slice(key));
    if (status.ok()) {
      return outcome::success();
    }

    return status_as_error(status, logger_);
  }

  void RocksDbSpace::compact(const ByteVec &first, const ByteVec &last) {
    auto rocks = storage_.lock();
    if (!rocks) {
      return;
    }
    if (rocks->db_) {
      std::unique_ptr<rocksdb::Iterator> begin(
          rocks->db_->NewIterator(rocks->ro_, column_));
      first.empty() ? begin->SeekToFirst() : begin->Seek(make_slice(first));
      auto bk = begin->key();
      std::unique_ptr<rocksdb::Iterator> end(
          rocks->db_->NewIterator(rocks->ro_, column_));
      last.empty() ? end->SeekToLast() : end->Seek(make_slice(last));
      auto ek = end->key();
      rocksdb::CompactRangeOptions options;
      rocks->db_->CompactRange(options, column_, &bk, &ek);
    }
  }

  outcome::result<std::shared_ptr<RocksDb>> RocksDbSpace::use() const {
    auto rocks = storage_.lock();
    if (!rocks) {
      return StorageError::STORAGE_GONE;
    }
    return rocks;
  }

}  // namespace lean::storage
