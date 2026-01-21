/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <ranges>

#include <sszpp/lists.hpp>

#include "types/constants.hpp"
#include "types/validator_index.hpp"

namespace lean {
  using AggregationBits = ssz::list<bool, VALIDATOR_REGISTRY_LIMIT>;

  inline auto getAggregatedValidators(const AggregationBits &aggregation_bits) {
    return std::views::iota(ValidatorIndex{0},
                            ValidatorIndex{aggregation_bits.size()})
         | std::views::filter([&](ValidatorIndex validator_index) {
             return aggregation_bits.data()[validator_index];
           });
  }

  inline bool hasAggregatedValidator(const AggregationBits &aggregation_bits,
                                     ValidatorIndex validator_index) {
    return validator_index < aggregation_bits.size()
       and aggregation_bits.data()[validator_index];
  }

  inline void addAggregatedValidator(AggregationBits &aggregation_bits,
                                     ValidatorIndex validator_index) {
    if (aggregation_bits.size() <= validator_index) {
      aggregation_bits.data().resize(validator_index + 1);
    }
    aggregation_bits.data()[validator_index] = true;
  }
}  // namespace lean
