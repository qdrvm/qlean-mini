#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(storage
    in_memory/in_memory_storage.cpp
    storage_error.cpp
    rocksdb/rocksdb.cpp
    rocksdb/rocksdb_batch.cpp
    rocksdb/rocksdb_cursor.cpp
    rocksdb/rocksdb_spaces.cpp
)

target_link_libraries(storage
    qtils::qtils
    RocksDB::rocksdb
    fd_limit
)

