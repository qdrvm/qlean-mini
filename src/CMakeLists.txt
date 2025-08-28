#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(qlean_headers INTERFACE)
target_include_directories(qlean_headers INTERFACE ${CMAKE_CURRENT_LIST_DIR})

add_executable(node node.cpp)
target_link_libraries(node
  qlean_headers
  sszpp
)
