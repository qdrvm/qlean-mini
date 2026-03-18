/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <variant>

#include <qtils/byte_vec.hpp>

#include "modules/networking/types.hpp"
#include "serde/json_fwd.hpp"
#include "types/aggregated_attestation.hpp"
#include "types/aggregated_signature_proof.hpp"
#include "types/attestation.hpp"
#include "types/attestation_data.hpp"
#include "types/block.hpp"
#include "types/block_body.hpp"
#include "types/block_header.hpp"
#include "types/block_signatures.hpp"
#include "types/block_with_attestation.hpp"
#include "types/checkpoint.hpp"
#include "types/config.hpp"
#include "types/signature.hpp"
#include "types/signed_attestation.hpp"
#include "types/signed_block_with_attestation.hpp"
#include "types/state.hpp"
#include "types/validator.hpp"

namespace lean {
  template <typename T>
  struct SszTestJsonT {
    T value;
    qtils::ByteVec serialized;

    JSON_FIELDS(value, serialized);
  };

  struct SszTestJson {
    struct Disabled : ssz::ssz_container {
      uint8_t nonce;
      SSZ_CONT(nonce);
    };

    // xmss internals
    using PublicKey = Disabled;

    std::variant<SszTestJsonT<AggregatedAttestation>,
                 SszTestJsonT<AggregatedSignatureProof>,
                 SszTestJsonT<Attestation>,
                 SszTestJsonT<AttestationData>,
                 SszTestJsonT<Block>,
                 SszTestJsonT<BlockBody>,
                 SszTestJsonT<BlockHeader>,
                 SszTestJsonT<BlockRequest>,
                 SszTestJsonT<BlockSignatures>,
                 SszTestJsonT<BlockWithAttestation>,
                 SszTestJsonT<Checkpoint>,
                 SszTestJsonT<Config>,
                 SszTestJsonT<PublicKey>,
                 SszTestJsonT<Signature>,
                 SszTestJsonT<SignedAttestation>,
                 SszTestJsonT<SignedBlockWithAttestation>,
                 SszTestJsonT<State>,
                 SszTestJsonT<StatusMessage>,
                 SszTestJsonT<Validator>>
        v;

    JSON_DISCRIMINATOR(type_name,
                       "AggregatedAttestation",
                       "AggregatedSignatureProof",
                       "Attestation",
                       "AttestationData",
                       "Block",
                       "BlockBody",
                       "BlockHeader",
                       "BlocksByRootRequest",
                       "BlockSignatures",
                       "BlockWithAttestation",
                       "Checkpoint",
                       "Config",
                       "PublicKey",
                       "Signature",
                       "SignedAttestation",
                       "SignedBlockWithAttestation",
                       "State",
                       "Status",
                       "Validator");

    auto &typeName() const {
      return typeFieldValues()[v.index()];
    }

    bool disabled() const {
      // `std::holds_alternative` doesn't work with duplicate types.
      return std::visit(
          [](const auto &t) {
            return std::is_same_v<std::remove_cvref_t<decltype(t)>,
                                  SszTestJsonT<Disabled>>;
          },
          v);
    }
  };
}  // namespace lean

namespace lean::json {
  struct JsonIn;

  void decode(const JsonIn &json, SszTestJson::Disabled &v) {}
}  // namespace lean::json
