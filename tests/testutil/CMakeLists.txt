#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(logger_for_tests INTERFACE)
target_link_libraries(logger_for_tests INTERFACE
    logger
    )

add_subdirectory(storage)
