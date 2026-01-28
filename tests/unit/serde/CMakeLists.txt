#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

addtest(enr_test
    enr_test.cpp
)
target_link_libraries(enr_test
    enr
)

addtest(snappy_test
    snappy_test.cpp
)
target_link_libraries(snappy_test
    p2p::p2p_varint_prefix_reader
    snappy
)

