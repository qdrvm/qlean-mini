#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(metrics
    impl/metrics_impl.cpp
    impl/prometheus/handler_impl.cpp
    impl/prometheus/metrics_impl.cpp
    impl/prometheus/registry_impl.cpp
    ../utils/tuner.cpp
)

target_link_libraries(metrics
    app_state_manager
    Boost::boost
    prometheus-cpp::core
)
