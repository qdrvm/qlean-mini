/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <mock/app/configuration_mock.hpp>
#include <qtils/test/outcome.hpp>

#include "app/configuration.hpp"
#include "storage/rocksdb/rocksdb.hpp"
#include "storage/storage_error.hpp"
#include "testutil/prepare_loggers.hpp"
#include "testutil/storage/base_fs_test.hpp"

using lean::app::ConfigurationMock;
using lean::log::LoggingSystem;
using lean::storage::RocksDb;
using DatabaseConfig = lean::app::Configuration::DatabaseConfig;
using namespace testing;

struct RocksDb_Open : public test::BaseFS_Test {
  RocksDb_Open() : test::BaseFS_Test("/tmp/lean-test-rocksdb-open") {}

  void SetUp() override {
    BaseFS_Test::SetUp();

    logsys = testutil::prepareLoggers();
    app_config = std::make_shared<ConfigurationMock>();

    DatabaseConfig db_config{
        .directory = getPathString() + "/db",
        .cache_size = 8 << 20,  // 8Mb
    };
    EXPECT_CALL(*app_config, database()).WillRepeatedly(ReturnRef(db_config));
  };

  void TearDown() override {
    app_config.reset();
    BaseFS_Test::TearDown();
  }

  std::shared_ptr<LoggingSystem> logsys;
  std::shared_ptr<ConfigurationMock> app_config;
};

/**
 * @given options with the disabled option `create_if_missing`
 * @when open database
 * @then database can not be opened (since there is no db already)
 */
TEST_F(RocksDb_Open, OpenNonExistingDB) {
  DatabaseConfig db_config{
      .directory = "/dev/zero/impossible/path",
      .cache_size = 8 << 20,  // 8Mb
  };

  EXPECT_CALL(*app_config, database()).WillRepeatedly(ReturnRef(db_config));

  ASSERT_THROW_OUTCOME(RocksDb(logsys, app_config), std::errc::not_a_directory);
}

/**
 * @given options with enable option `create_if_missing`
 * @when open database
 * @then database is opened
 */
TEST_F(RocksDb_Open, OpenExistingDB) {
  DatabaseConfig db_config{
      .directory = getPathString() + "/db",
      .cache_size = 8 << 20,  // 8Mb
  };

  EXPECT_CALL(*app_config, database()).WillRepeatedly(ReturnRef(db_config));

  ASSERT_NO_THROW(RocksDb(logsys, app_config));
}
