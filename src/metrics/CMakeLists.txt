#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(metrics
    impl/exposer_impl.cpp
    impl/prometheus/handler_impl.cpp
    impl/prometheus/metrics_impl.cpp
    impl/prometheus/registry_impl.cpp
    impl/session_impl.cpp
    ../utils/tuner.cpp
)

target_link_libraries(metrics
    app_state_manager
    Boost::boost
    prometheus-cpp::core
)

add_library(metrics_watcher
    impl/metrics_watcher.cpp
)

target_link_libraries(metrics_watcher
    metrics
)

