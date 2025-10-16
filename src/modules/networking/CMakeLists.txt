#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_lean_module(networking
  SOURCE
    block_request_protocol.cpp
    networking.cpp
    status_protocol.cpp
  INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
  DEFINITIONS
    SOME_FLAG=1
  LIBRARIES
    blockchain
    bootnodes
    p2p::libp2p
    qtils::qtils
    Snappy::snappy
    soralog::soralog
    sszpp
    validator_registry
)
