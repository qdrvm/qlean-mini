#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(node_injector
    node_injector.cpp
)
target_link_libraries(node_injector
    Boost::Boost.DI
    logger
    app_configurator
    chain_spec
    app_state_manager
    application
    metrics
    clock
    hasher
    se_async
    modules
    storage
    blockchain
)
