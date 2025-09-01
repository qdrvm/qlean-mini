#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_subdirectory(blake2)
add_subdirectory(sha)
add_subdirectory(twox)

add_library(hasher
    hasher/hasher_impl.cpp
)
target_link_libraries(hasher
    blake2
    twox
    sha
    keccak
)
