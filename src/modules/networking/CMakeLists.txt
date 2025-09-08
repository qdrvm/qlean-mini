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
    p2p::libp2p
    qtils::qtils
    soralog::soralog
    sszpp
)