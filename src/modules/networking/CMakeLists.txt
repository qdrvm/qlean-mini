#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_lean_module(networking
  SOURCE
    networking.cpp
  INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
  DEFINITIONS
    SOME_FLAG=1
  LIBRARIES
    qtils::qtils
    soralog::soralog
    hashtree
)