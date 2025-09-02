/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "testutil/storage/base_rocksdb_test.hpp"

namespace test {

  void BaseRocksDB_Test::open() {
    rocksdb::Options options;
    options.create_if_missing = true;

    rocks_.reset();
    ASSERT_NO_THROW(
        rocks_ = std::make_shared<lean::storage::RocksDb>(logsys, app_config));

    db_ = rocks_->getSpace(lean::storage::Space::Default);
    ASSERT_TRUE(db_) << "BaseRocksDB_Test: db is nullptr";
  }

  BaseRocksDB_Test::BaseRocksDB_Test(fs::path path)
      : BaseFS_Test(std::move(path)) {}

  void BaseRocksDB_Test::SetUp() {
    logsys = testutil::prepareLoggers();
    app_config = std::make_shared<lean::app::ConfigurationMock>();

    lean::app::Configuration::DatabaseConfig db_config{
        .directory = getPathString() + "/db",
        .cache_size = 8 << 20,  // 8Mb
    };
    EXPECT_CALL(*app_config, database())
        .WillRepeatedly(testing::ReturnRef(db_config));

    open();
  }

  void BaseRocksDB_Test::TearDown() {
    app_config.reset();
    clear();
  }

}  // namespace test
