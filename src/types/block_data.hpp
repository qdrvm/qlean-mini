/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sszpp/basic_types.hpp>
#include <sszpp/lists.hpp>

#include "types/attestation.hpp"
#include "types/block_body.hpp"
#include "types/block_header.hpp"
#include "types/block_signatures.hpp"
#include "types/types.hpp"

// SSZ-friendly optional encoded as List[T; max=1].
// Empty list -> None, single element -> Some(T).
template <typename T>
struct ssz_maybe : public ssz::ssz_container {
  ssz::list<T, 1> inner;  // length 0 or 1

  SSZ_CONT(inner);

  // Convenience helpers
  bool has_value() const {
    return inner.size() == 1;
  }
  explicit operator bool() const {
    return has_value();
  }

  void reset() {
    inner.clear();
  }
  void emplace(const T &v) {
    inner = {};
    inner.push_back(v);
  }
  void emplace() {
    inner = {};
    inner.push_back({});
  }

  T &value() {
    return inner[0];
  }
  const T &value() const {
    return inner[0];
  }

  T &operator*() {
    return value();
  }
  const T &operator*() const {
    return value();
  }

  T *operator->() {
    return &value();
  }
  const T *operator->() const {
    return &value();
  }
};


namespace lean {
  struct BlockData : ssz::ssz_variable_size_container {
    BlockHash hash;
    ssz_maybe<BlockHeader> header;
    ssz_maybe<BlockBody> body;
    ssz_maybe<Attestation> attestation;
    ssz_maybe<BlockSignatures> signature;

    SSZ_CONT(hash, header, body, attestation, signature);
  };
}  // namespace lean
