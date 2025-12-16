#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

addtest(test_vectors_test
    test_vectors_test.cpp
)
target_link_libraries(test_vectors_test
    blockchain
)
target_compile_definitions(test_vectors_test PRIVATE
    PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
