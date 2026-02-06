#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

set(CMAKE_OSX_ARCHITECTURES "arm64")

set(LIBRARIES
    node_injector
)

include_directories(
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/qlean_enable_shadow.hpp.in
    ${CMAKE_BINARY_DIR}/generated/executable/qlean_enable_shadow.hpp
)

# fix for dyld bug on mac
# we should actually check the target platform here, not the host platform,
# but not that we explicitly support cross-compilation anyway
if (CMAKE_HOST_APPLE)
  option(MACOS_BIG_EXE_USE_DYLIB "compile dylib instead of exe to work around dyld cache error when loading big exe" ON)
endif ()
if (MACOS_BIG_EXE_USE_DYLIB)
  add_library(lean_node SHARED lean_node.cpp)
  set_target_properties(lean_node PROPERTIES PREFIX "" DEBUG_POSTFIX "")
  target_compile_definitions(lean_node PRIVATE BUILD_AS_LIBRARY)
  target_link_libraries(lean_node ${LIBRARIES})

  add_executable(lean_node_dlopen dlopen.cpp)
  set_target_properties(lean_node_dlopen PROPERTIES OUTPUT_NAME qlean)
  set_target_properties(lean_node_dlopen PROPERTIES LINKER_LANGUAGE CXX)
  add_dependencies(lean_node_dlopen lean_node)
else ()
  add_executable(lean_node lean_node.cpp)
  target_link_libraries(lean_node ${LIBRARIES})
endif ()
set_target_properties(lean_node PROPERTIES OUTPUT_NAME qlean)
add_dependencies(lean_node all_modules)

#if (BACKWARD)
#  add_backward(lean_node)
#endif ()


add_executable(experiment experiment.cpp)
target_link_libraries(experiment fmt::fmt)
