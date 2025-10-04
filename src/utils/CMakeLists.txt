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

add_library(validator_registry
    validator_registry.cpp
)
target_link_libraries(validator_registry
    yaml-cpp::yaml-cpp
    logger
    fmt::fmt
    app_configuration
)
