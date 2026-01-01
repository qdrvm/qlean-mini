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
struct fmt::formatter<lean::Debug<lean::AttestationData>> {
  constexpr auto parse(const auto &ctx) {
    return ctx.end();
  }
  auto format(const lean::Debug<lean::AttestationData> &v,
              fmt::format_context &ctx) const {
    return fmt::format_to(ctx.out(),
                          "[{},{},{},{}]",
                          v.v.slot,
                          v.v.head.slot,
                          v.v.target.slot,
                          v.v.source.slot);
  }
};
template <>
struct fmt::formatter<lean::Debug<lean::Attestation>> {
  constexpr auto parse(const auto &ctx) {
    return ctx.end();
  }
  auto format(const lean::Debug<lean::Attestation> &v,
              fmt::format_context &ctx) const {
    return fmt::format_to(
        ctx.out(), "[{},{}]", v.v.validator_id, lean::Debug{v.v.data});
  }
};

template <>
struct fmt::formatter<lean::Debug<lean::AggregatedAttestation>> {
  constexpr auto parse(const auto &ctx) {
    return ctx.end();
  }
  auto format(const lean::Debug<lean::AggregatedAttestation> &v,
              fmt::format_context &ctx) const {
    return fmt::format_to(
        ctx.out(),
        "[[{}],{}]",
        fmt::join(lean::getAggregatedValidators(v.v.aggregation_bits), ","),
        lean::Debug{v.v.data});
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
                      | std::views::transform(
                          [](const lean::AggregatedAttestation &v) {
                            return lean::Debug{v};
                          }),
                  "-"),
        lean::Debug{v.v.proposer_attestation});
  }
};
