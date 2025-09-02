#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_jam_module(example
  SOURCE
    example.cpp
  INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src
  DEFINITIONS
    SOME_FLAG=1
  LIBRARIES
    qtils::qtils
    soralog::soralog
)