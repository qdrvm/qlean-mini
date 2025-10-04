#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

#addtest(chain_spec_test
#    chain_spec_test.cpp
#    )
#target_link_libraries(chain_spec_test
#    chain_spec
#    logger_for_tests
#    )

addtest(bootnodes_test
    bootnodes_test.cpp
    )
target_link_libraries(bootnodes_test
    bootnodes
    chain_spec
    enr
    logger_for_tests
    p2p::libp2p
    )

addtest(app_state_manager_test
    state_manager_test.cpp
    )
target_link_libraries(app_state_manager_test
    app_state_manager
    logger_for_tests
    )

#addtest(app_config_test
#    config_test.cpp
#    )
#target_link_libraries(app_config_test
#    app_config
#    filesystem
#    logger_for_tests
#    telemetry
#    )
