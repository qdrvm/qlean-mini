#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

addtest(block_storage_test
    block_storage_test.cpp
    )
target_link_libraries(block_storage_test
    blockchain #???
    logger_for_tests
    storage
    )