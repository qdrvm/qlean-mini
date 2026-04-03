#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

lean_add_library_shared(fd_limit
    fd_limit.cpp
)
target_link_libraries(fd_limit
    Boost::boost
    logger
)

lean_add_library_shared(http
    http.cpp
)
target_link_libraries(http
    Boost::boost
    logger
)
