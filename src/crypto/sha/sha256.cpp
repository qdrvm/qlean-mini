/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "crypto/sha/sha256.hpp"

#include <openssl/sha.h>

namespace lean::crypto {
  Hash256 sha256(std::string_view input) {
    return sha256(
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        {reinterpret_cast<const uint8_t *>(input.data()), input.size()});
  }

  Hash256 sha256(qtils::ByteView input) {
    Hash256 out;
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, input.data(), input.size());
    SHA256_Final(out.data(), &ctx);
    return out;
  }
}  // namespace lean::crypto
