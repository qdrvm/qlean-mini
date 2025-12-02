#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(node_injector
    node_injector.cpp
)
target_link_libraries(node_injector
    app_configurator
    app_state_manager
    application
    blockchain
    Boost::Boost.DI
    chain_spec
    clock
    hasher
    logger
    metrics
    modules
    p2p::libp2p
    se_async
    storage
    timeline
    p2p::p2p_key_validator
    validator_keys_manifest
    validator_registry
    xmss_provider
)
