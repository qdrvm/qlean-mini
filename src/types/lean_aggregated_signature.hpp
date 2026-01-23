/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/lists.hpp>

namespace lean {
  // https://github.com/leanEthereum/leanSpec/blob/6430aaf7cb505ec76bb1af6045ffcd3b41899e42/src/lean_spec/subspecs/xmss/aggregation.py#L57-L58
  constexpr size_t kMaxLeanAggregatedSignatureSize = 1 << 20;

  using LeanAggregatedSignature =
      ssz::list<uint8_t, kMaxLeanAggregatedSignatureSize>;
}  // namespace lean
