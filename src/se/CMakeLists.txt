#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(se_async
    async_dispatcher.cpp
)

target_link_libraries(se_async
    logger
    fmt::fmt
)

add_library(se_sync
    sync_dispatcher.cpp
)

target_link_libraries(se_sync
    logger
    fmt::fmt
)
