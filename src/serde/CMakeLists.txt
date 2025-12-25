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
    p2p::p2p_key_validator
)

add_library(snappy INTERFACE)
target_link_libraries(snappy INTERFACE
    Crc32c::crc32c
    qtils::qtils
    Snappy::snappy
)
