#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

addtest(xmss_provider_test
    xmss_provider_test.cpp
)
target_link_libraries(xmss_provider_test
    xmss_provider
)
