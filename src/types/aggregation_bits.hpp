/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <ranges>

#include <sszpp/lists.hpp>
#include <sszpp/wrapper.hpp>

#include "types/constants.hpp"
#include "types/validator_index.hpp"

namespace lean {
  struct AggregationBits : ssz::ssz_variable_size_container {
    auto iter() const {
      return std::views::iota(ValidatorIndex{0}, ValidatorIndex{bits.size()})
           | std::views::filter([this](ValidatorIndex validator_index) {
               return bits.data()[validator_index];
             });
    }

    bool contains(ValidatorIndex validator_index) const {
      return validator_index < bits.size() and bits.data()[validator_index];
    }

    void add(ValidatorIndex validator_index) {
      if (bits.size() <= validator_index) {
        bits.data().resize(validator_index + 1);
      }
      bits.data()[validator_index] = true;
    }

    ssz::list<bool, VALIDATOR_REGISTRY_LIMIT> bits;

    SSZ_WRAPPER(bits);
    bool operator==(const AggregationBits &) const = default;
  };
}  // namespace lean
