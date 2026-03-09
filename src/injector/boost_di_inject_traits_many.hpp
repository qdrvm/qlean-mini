/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

// `BOOST_DI_INJECT_TRAITS` breaks if constructor has more than 10 arguments.
#define BOOST_DI_INJECT_TRAITS_MANY(...)                             \
  struct boost_di_inject__ {                                         \
    static void ctor(__VA_ARGS__);                                   \
    using type __BOOST_DI_UNUSED =                                   \
        ::boost::di::v1_1_0::aux::function_traits_t<decltype(ctor)>; \
  }
