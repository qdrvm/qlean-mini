#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(enr
    enr.cpp
)
target_link_libraries(enr
    cppcodec
    p2p::libp2p
)
