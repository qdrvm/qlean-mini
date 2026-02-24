#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(fd_limit SHARED
    fd_limit.cpp
)
target_link_libraries(fd_limit
    Boost::boost
    logger
)

add_library(http SHARED
    http.cpp
)
target_link_libraries(http
    Boost::boost
    logger
)
