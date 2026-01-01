/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <fmt/format.h>

#include "types/block_with_attestation.hpp"

namespace lean {
  template <typename T>
  struct Debug {
    const T &v;
  };
}  // namespace lean

template <>
struct fmt::formatter<lean::Debug<lean::Attestation>> {
  constexpr auto parse(const auto &ctx) {
    return ctx.end();
  }
  auto format(const lean::Debug<lean::Attestation> &v,
              fmt::format_context &ctx) const {
    return fmt::format_to(ctx.out(),
                          "[{}-{}-{}-{}-{}]",
                          v.v.validator_id,
                          v.v.data.slot,
                          v.v.data.head.slot,
                          v.v.data.target.slot,
                          v.v.data.source.slot);
  }
};

template <>
struct fmt::formatter<lean::Debug<lean::BlockWithAttestation>> {
  constexpr auto parse(const auto &ctx) {
    return ctx.end();
  }
  auto format(const lean::Debug<lean::BlockWithAttestation> &v,
              fmt::format_context &ctx) const {
    return fmt::format_to(
        ctx.out(),
        "[{}-{}-{}-[{}]-{}]",
        v.v.block.slot,
        v.v.block.hash(),
        v.v.block.parent_root,
        fmt::join(v.v.block.body.attestations.data()
                      | std::views::transform([](const lean::Attestation &v) {
                          return lean::Debug{v};
                        }),
                  "-"),
        lean::Debug{v.v.proposer_attestation});
  }
};
