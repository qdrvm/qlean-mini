#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.25)

option(TESTING "Build and run test suite" ON)
if (TESTING)
  list(APPEND VCPKG_MANIFEST_FEATURES test)
endif ()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API ON)

project(qlean-mini
    VERSION 0.0.1
    LANGUAGES CXX C
)

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "Boost_DIR: ${Boost_DIR}")

find_package(Python3 REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(libb2 REQUIRED IMPORTED_TARGET GLOBAL libb2)

find_package(Boost CONFIG REQUIRED COMPONENTS algorithm outcome program_options property_tree)
find_package(fmt CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(sszpp CONFIG REQUIRED)
find_package(soralog CONFIG REQUIRED)
find_package(Boost.DI CONFIG REQUIRED)
find_package(qtils CONFIG REQUIRED)
find_package(prometheus-cpp CONFIG REQUIRED)
find_package(OpenSSL CONFIG REQUIRED)
find_package(RocksDB CONFIG REQUIRED)
find_package(hashtree CONFIG REQUIRED)

# TODO Temporarily commented out until gcc is updated (gcc-13 crashes because of this).
# if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
#   add_compile_options(-fmodules-ts)
# elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
#   add_compile_options(-fmodules)
# endif()

add_library(headers INTERFACE)
target_include_directories(headers INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/third_party)
include_directories(${CMAKE_SOURCE_DIR}/src/_TODO)
include_directories(${CMAKE_BINARY_DIR}/generated)

add_subdirectory(src)

if (TESTING)
  enable_testing()

  find_package(GTest CONFIG REQUIRED)
  set(GTEST_DEPS GTest::gtest_main)

  add_subdirectory(tests)
endif ()
