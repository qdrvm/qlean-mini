#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

addtest(validator_registry_test
    validator_registry_test.cpp
)
target_link_libraries(validator_registry_test
    validator_registry
    logger_for_tests
)

addtest(lru_cache_test
    lru_cache_test.cpp
)
target_link_libraries(lru_cache_test
    qtils::qtils
)
