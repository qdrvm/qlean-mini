/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <crypto/hash_types.hpp>
#include <qtils/byte_view.hpp>

namespace lean::crypto {

  Hash64 make_twox64(qtils::ByteView buf);

  Hash128 make_twox128(qtils::ByteView buf);

  Hash256 make_twox256(qtils::ByteView buf);

}  // namespace lean::crypto
