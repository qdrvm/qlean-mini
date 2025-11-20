/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <optional>

#include "crypto/xmss/types.hpp"
#include "types/validator_index.hpp"

namespace lean::app {

  class ValidatorKeysManifest {
   public:
    virtual ~ValidatorKeysManifest() = default;
    [[nodiscard]] virtual std::optional<crypto::xmss::XmssPublicKey>
    getXmssPubkeyByIndex(ValidatorIndex index) const = 0;

    [[nodiscard]] virtual crypto::xmss::XmssKeypair currentNodeXmssKeypair()
        const = 0;

    [[nodiscard]] virtual std::vector<crypto::xmss::XmssPublicKey>
    getAllXmssPubkeys() const = 0;
  };
}  // namespace lean::app
