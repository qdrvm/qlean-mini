/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fmt/format.h>

namespace lean {

  class RequestId {
   public:
    RequestId(size_t seed, size_t n)
        : id(n + 0x9e3779b9 + (seed << 6) + (seed >> 2)) {};

    RequestId(const RequestId &) = default;

    bool operator==(const RequestId &) const = default;

   private:
    friend struct fmt::formatter<RequestId>;
    friend struct std::hash<RequestId>;
    size_t id;
  };

  class RequestCxt {
   public:
    RequestCxt(const RequestId &rid) : rid(rid) {};

    RequestId rid;
  };

}  // namespace lean

template <>
struct std::hash<lean::RequestId> {
  size_t operator()(const lean::RequestId &id) const noexcept {
    return id.id;
  }
};


template <>
struct fmt::formatter<lean::RequestId> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const lean::RequestId &rid, FormatContext &ctx) const
      -> decltype(ctx.out()) {
    auto *b = reinterpret_cast<const uint8_t *>(&rid.id);
    return fmt::format_to(ctx.out(), "{:02x}{:02x}{:02x}", b[0], b[1], b[2]);
  }
};
