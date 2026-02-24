#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

execute_process(
    COMMAND git symbolic-ref -q HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_REF
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(GIT_REF)
  set(GIT_REF_FILE "${CMAKE_SOURCE_DIR}/.git/${GIT_REF}")
else()
  set(GIT_REF_FILE "${CMAKE_SOURCE_DIR}/.git/HEAD")
endif()

set(BUILD_VERSION_CPP "${CMAKE_BINARY_DIR}/generated/app/build_version.cpp")
set(GET_VERSION_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/get_version.sh")
add_custom_command(
    OUTPUT ${BUILD_VERSION_CPP}

    COMMAND ${CMAKE_COMMAND} -E make_directory "$<SHELL_PATH:${CMAKE_BINARY_DIR}/generated/app>"

    COMMAND ${CMAKE_COMMAND} -E echo "// Auto-generated file" > "${BUILD_VERSION_CPP}"
    COMMAND ${CMAKE_COMMAND} -E echo "#include <string>" >> "${BUILD_VERSION_CPP}"
    COMMAND ${CMAKE_COMMAND} -E echo "" >> "${BUILD_VERSION_CPP}"
    COMMAND ${CMAKE_COMMAND} -E echo "namespace lean {" >> "${BUILD_VERSION_CPP}"
    COMMAND ${CMAKE_COMMAND} -E echo "  const std::string &buildVersion() {" >> "${BUILD_VERSION_CPP}"
    COMMAND ${CMAKE_COMMAND} -E env sh -c "printf '%s' '    static const std::string buildVersion(\"' >> \"${BUILD_VERSION_CPP}\""

    COMMAND ${CMAKE_COMMAND} -E env sh -c "\"${GET_VERSION_SCRIPT}\" >> \"${BUILD_VERSION_CPP}\""

    COMMAND ${CMAKE_COMMAND} -E echo "\");" >> "${BUILD_VERSION_CPP}"
    COMMAND ${CMAKE_COMMAND} -E echo "    return buildVersion;" >> "${BUILD_VERSION_CPP}"
    COMMAND ${CMAKE_COMMAND} -E echo "  }" >> "${BUILD_VERSION_CPP}"
    COMMAND ${CMAKE_COMMAND} -E echo "}  // namespace lean" >> "${BUILD_VERSION_CPP}"

    COMMENT "Generate build_version.cpp"
    DEPENDS
      "${GET_VERSION_SCRIPT}"
      "${GIT_REF_FILE}"
    VERBATIM
)
add_library(build_version
    "${BUILD_VERSION_CPP}"
)

set(DEFAULT_CONFIG_CPP  "${CMAKE_BINARY_DIR}/generated/app/default_config.cpp")
set(DEFAULT_CONFIG_YAML "${CMAKE_SOURCE_DIR}/example/config.yaml")
add_custom_command(
    OUTPUT "${DEFAULT_CONFIG_CPP}"

    COMMAND ${CMAKE_COMMAND} -E make_directory "$<SHELL_PATH:${CMAKE_BINARY_DIR}/generated/app>"

    COMMAND ${CMAKE_COMMAND} -E echo "// Auto-generated file" > "${DEFAULT_CONFIG_CPP}"
    COMMAND ${CMAKE_COMMAND} -E echo "#include \"app/default_config.hpp\"" >> "${DEFAULT_CONFIG_CPP}"
    COMMAND ${CMAKE_COMMAND} -E echo "" >> "${DEFAULT_CONFIG_CPP}"
    COMMAND ${CMAKE_COMMAND} -E echo "namespace lean {" >> "${DEFAULT_CONFIG_CPP}"
    COMMAND ${CMAKE_COMMAND} -E echo "  const std::string &defaultConfigYaml() {" >> "${DEFAULT_CONFIG_CPP}"
    COMMAND ${CMAKE_COMMAND} -E echo "    static const std::string defaultConfigYaml = R\"yaml(" >> "${DEFAULT_CONFIG_CPP}"

    COMMAND ${CMAKE_COMMAND} -E cat "${DEFAULT_CONFIG_YAML}" >> "${DEFAULT_CONFIG_CPP}"

    COMMAND ${CMAKE_COMMAND} -E echo "" >> "${DEFAULT_CONFIG_CPP}"
    COMMAND ${CMAKE_COMMAND} -E echo ")yaml\";" >> "${DEFAULT_CONFIG_CPP}"
    COMMAND ${CMAKE_COMMAND} -E echo "    return defaultConfigYaml;" >> "${DEFAULT_CONFIG_CPP}"
    COMMAND ${CMAKE_COMMAND} -E echo "  }" >> "${DEFAULT_CONFIG_CPP}"
    COMMAND ${CMAKE_COMMAND} -E echo "}  // namespace lean" >> "${DEFAULT_CONFIG_CPP}"

    DEPENDS "${DEFAULT_CONFIG_YAML}"
    COMMENT "Generate default_config.cpp"
    VERBATIM
)

add_library(app_configuration SHARED
    configuration.cpp
    "${DEFAULT_CONFIG_CPP}"
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
    Boost::program_options
    build_version
    xmss_provider
    p2p::libp2p
    yaml-cpp::yaml-cpp
)

add_library(app_state_manager SHARED
    impl/state_manager_impl.cpp
)
target_link_libraries(app_state_manager
    qtils::qtils
    logger
)

add_library(bootnodes SHARED
    bootnodes.cpp
)
target_link_libraries(bootnodes
    p2p::libp2p
)

add_library(chain_spec SHARED
    impl/chain_spec_impl.cpp
)
target_link_libraries(chain_spec
    bootnodes
    enr
    logger
    Boost::property_tree
    app_configuration
    sszpp
    p2p::libp2p
    yaml-cpp::yaml-cpp
)

add_library(application SHARED
    impl/application_impl.cpp
    impl/http_server.cpp
)
target_link_libraries(application
    qtils::qtils
    app_configuration
    blockchain
    http
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

add_library(validator_keys_manifest SHARED
    impl/validator_keys_manifest_impl.cpp
)
target_link_libraries(validator_keys_manifest
    app_configuration
    qtils::qtils
    yaml-cpp::yaml-cpp
)
