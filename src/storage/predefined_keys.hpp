/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/byte_vec.hpp>
#include <qtils/literals.hpp>

namespace lean::storage {

  using qtils::literals::operator""_vec;

  inline const qtils::ByteVec kBlockTreeLeavesLookupKey =
      ":lean:block_tree_leaves"_vec;

}  // namespace lean::storage
