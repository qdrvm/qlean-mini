/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/byte_arr.hpp>

namespace lean {
  // https://github.com/ReamLabs/ream/blob/5a4b3cb42d5646a0d12ec1825ace03645dbfd59b/crates/common/consensus/lean/src/vote.rs#L13-L18
  using VoteSignature = qtils::ByteArr<4000>;
}
