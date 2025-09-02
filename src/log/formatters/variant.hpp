/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <boost/variant.hpp>
#include <fmt/core.h>

#include "common/visitor.hpp"

template <typename... Args>
struct fmt::formatter<boost::variant<Args...>> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const boost::variant<Args...> &variant, FormatContext &ctx) const
      -> decltype(ctx.out()) {
    // ctx.out() is an output iterator to write to.

    return lean::visit_in_place(variant, [&](const auto &value) {
      return fmt::format_to(ctx.out(), "{}", value);
    });
  }
};
