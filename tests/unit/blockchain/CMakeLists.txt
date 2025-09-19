#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

addtest(block_storage_test
    block_storage_test.cpp
    )
target_link_libraries(block_storage_test
    blockchain #???
    logger_for_tests
    storage
    )

addtest(fork_choice_test
    fork_choice_test.cpp
    )
target_link_libraries(fork_choice_test
    blockchain
    )

addtest(state_transition_function_test
    state_transition_function_test.cpp
    )
target_link_libraries(state_transition_function_test
    blockchain
    )
