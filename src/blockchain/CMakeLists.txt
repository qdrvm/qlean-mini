#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(blockchain
    impl/storage_util.cpp
    impl/block_storage_impl.cpp
    impl/block_storage_error.cpp
    impl/block_storage_initializer.cpp
)
target_link_libraries(blockchain
    Boost::boost
    sszpp
)
