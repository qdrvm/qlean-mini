#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(validator_registry
    impl/validator_registry_impl.cpp
)
target_link_libraries(validator_registry
    app_configuration
    fmt::fmt
    logger
    yaml-cpp::yaml-cpp
)

add_library(blockchain
    fork_choice.cpp
    genesis_config.cpp
    impl/anchor_block_impl.cpp
    impl/anchor_state_impl.cpp
    impl/block_storage_error.cpp
    impl/block_storage_impl.cpp
    impl/block_storage_initializer.cpp
    impl/block_tree_error.cpp
    impl/block_tree_impl.cpp
    impl/block_tree_initializer.cpp
    impl/cached_tree.cpp
    impl/storage_util.cpp
    state_transition_function.cpp
)
target_link_libraries(blockchain
    Boost::boost
    metrics
    sszpp
    validator_registry
)
