#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(logger
    logger.cpp
)
target_link_libraries(logger
    fmt::fmt
    soralog::soralog
    soralog::configurator_yaml
    qtils::qtils
    Boost::algorithm
    )
