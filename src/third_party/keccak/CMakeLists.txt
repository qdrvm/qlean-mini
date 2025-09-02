#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(keccak
    keccak.c
)
set_source_files_properties(keccak.c PROPERTIES LANGUAGE C)
target_link_libraries(keccak
    OpenSSL::Crypto
    qtils::qtils
)
