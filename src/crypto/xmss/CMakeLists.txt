#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(xmss_provider
    xmss_provider_impl.cpp
)
target_link_libraries(xmss_provider
    c_hash_sig::c_hash_sig
)