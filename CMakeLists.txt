#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.25)
project(qlean_mini)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(hashtree CONFIG REQUIRED)
find_package(sszpp CONFIG REQUIRED)

add_subdirectory(src)
