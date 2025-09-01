/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "mock/app/configuration_mock.hpp"
#include "storage/rocksdb/rocksdb.hpp"
#include "testutil/storage/base_fs_test.hpp"

namespace test {

  struct BaseRocksDB_Test : public BaseFS_Test {
    using RocksDB = lean::storage::RocksDb;
    using Buffer = qtils::ByteVec;
    using BufferView = qtils::ByteView;

    BaseRocksDB_Test(fs::path path);

    void open();

    void SetUp() override;

    void TearDown() override;

    std::shared_ptr<lean::log::LoggingSystem> logsys;
    std::shared_ptr<lean::app::ConfigurationMock> app_config;

    std::shared_ptr<RocksDB> rocks_;
    std::shared_ptr<lean::storage::BufferStorage> db_;
  };

}  // namespace test
