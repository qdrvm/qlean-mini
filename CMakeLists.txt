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

find_package(Boost CONFIG REQUIRED filesystem random beast program_options)
find_package(Boost.DI CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(libp2p CONFIG REQUIRED)
find_package(libsecp256k1 CONFIG REQUIRED)
find_package(lsquic CONFIG REQUIRED)
find_package(OpenSSL CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(soralog CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

add_subdirectory(src)
