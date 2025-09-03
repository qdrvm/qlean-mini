#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

set(BUILD_VERSION_CPP "${CMAKE_BINARY_DIR}/generated/app/build_version.cpp")
set(GET_VERSION_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/get_version.sh")
add_custom_command(
    OUTPUT ${BUILD_VERSION_CPP}
    COMMAND echo "// Auto-generated file" > ${BUILD_VERSION_CPP}
    COMMAND echo "#include <string>" >> ${BUILD_VERSION_CPP}
    COMMAND echo "namespace lean {" >> ${BUILD_VERSION_CPP}
    COMMAND echo "  const std::string &buildVersion() {" >> ${BUILD_VERSION_CPP}
    COMMAND printf "    static const std::string buildVersion(\"" >> ${BUILD_VERSION_CPP}
    COMMAND ${GET_VERSION_SCRIPT} >> ${BUILD_VERSION_CPP}
    COMMAND echo "\");" >> ${BUILD_VERSION_CPP}
    COMMAND echo "    return buildVersion;" >> ${BUILD_VERSION_CPP}
    COMMAND echo "  }" >> ${BUILD_VERSION_CPP}
    COMMAND echo "}" >> ${BUILD_VERSION_CPP}
    COMMENT "Generate build_version.cpp"
    DEPENDS ${GET_VERSION_SCRIPT}
    VERBATIM
)
add_library(build_version
    ${CMAKE_BINARY_DIR}/generated/app/build_version.cpp
)

add_library(app_configuration SHARED
    configuration.cpp
)
target_link_libraries(app_configuration
    Boost::boost
    fmt::fmt
)

add_library(app_configurator SHARED
    configurator.cpp
)
target_link_libraries(app_configurator
    app_configuration
    yaml-cpp::yaml-cpp
    Boost::program_options
    build_version
)

add_library(app_state_manager SHARED
    impl/state_manager_impl.cpp
)
target_link_libraries(app_state_manager
    qtils::qtils
    logger
)

add_library(chain_spec SHARED
    impl/chain_spec_impl.cpp
)
target_link_libraries(chain_spec
    logger
    Boost::property_tree
    app_configuration
)

add_library(application SHARED
    impl/application_impl.cpp
)
target_link_libraries(application
    qtils::qtils
    app_configuration
    metrics
)

add_library(timeline SHARED
    impl/timeline_impl.cpp
)
target_link_libraries(timeline
    qtils::qtils
    logger
    sszpp
#    app_configuration
#    metrics
)
