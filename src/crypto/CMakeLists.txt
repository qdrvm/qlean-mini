#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

#add_subdirectory(blake2)
add_subdirectory(keystore)
add_subdirectory(sha)
#add_subdirectory(twox)
add_subdirectory(xmss)

add_library(hasher
    hasher/hasher_impl.cpp
)
target_link_libraries(hasher
    qtils::qtils
#    blake2
#    twox
    sha
#    keccak
)
