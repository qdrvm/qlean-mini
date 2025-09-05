#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

if(NOT TARGET all_modules)
  add_custom_target(all_modules)
endif()

function(add_lean_module NAME)
  set(MODULE_NAME ${NAME})

  set(MODULE "${MODULE_NAME}_module")

  # Parse named arguments
  cmake_parse_arguments(
      # Prefix for parsed argument variables
      MODULE
      # List of flags (boolean arguments without values)
      ""
      # List of named arguments with a single value
      ""
      # List of named arguments with multiple values
      "SOURCE;INCLUDE_DIRS;LIBRARIES;DEFINITIONS"
      # Input arguments
      ${ARGN}
  )

  if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/module.cpp)
    message(FATAL_ERROR "Not found `module.cpp` file (main file of module)")
  endif ()
  if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}.hpp" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}.cpp")
    message(FATAL_ERROR "Not found `${MODULE_NAME}.hpp` nor `${MODULE_NAME}.cpp` file (class of module)")
  endif ()

  # Create a shared module library
  add_library(${MODULE} MODULE # or SHARED
      module.cpp
      ${MODULE_NAME}.cpp
      ${MODULE_SOURCE}
  )

  # Set exported symbols visibility
  set_target_properties(${MODULE} PROPERTIES
      CXX_VISIBILITY_PRESET hidden
      VISIBILITY_INLINES_HIDDEN ON
  )

  # Set include directories
  if (MODULE_INCLUDE_DIRS)
    target_include_directories(${MODULE} PRIVATE
        ${MODULE_INCLUDE_DIRS}
    )
  endif ()

  # Set definitions specified for module
  if (MODULE_DEFINITIONS)
    target_compile_definitions(${MODULE} PRIVATE
        ${MODULE_DEFINITIONS}
    )
  endif ()

  # Link with libs
  if (MODULE_LIBRARIES)
    target_link_libraries(${MODULE}
        ${MODULE_LIBRARIES}
    )
  endif ()

  # Set C++ standard
  target_compile_features(${MODULE} PRIVATE
      cxx_std_23
  )

  add_dependencies(all_modules ${MODULE})

endfunction()

# -------------- Core-part of module subsystem --------------

add_library(modules
    module_loader.cpp
)

target_link_libraries(modules
    qtils::qtils
)

# -------------- Modules --------------

# Example module
add_subdirectory(example)

# Networking module
add_subdirectory(networking)

# Block-production module
add_subdirectory(production)

# Blockchain synchronizer module
add_subdirectory(synchronizer)
