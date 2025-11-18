#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(keystore
    keystore_impl.cpp
)
target_link_libraries(keystore
    xmss_provider
    qtils::qtils
)
