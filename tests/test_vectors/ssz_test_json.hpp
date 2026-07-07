/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>
#include <variant>

#include <qtils/byte_vec.hpp>

#include "modules/networking/types.hpp"
#include "serde/json.hpp"
#include "types/aggregated_attestation.hpp"
#include "types/aggregated_signature_proof.hpp"
#include "types/attestation.hpp"
#include "types/attestation_data.hpp"
#include "types/block.hpp"
#include "types/block_body.hpp"
#include "types/block_header.hpp"
#include "types/block_signatures.hpp"
#include "types/checkpoint.hpp"
#include "types/config.hpp"
#include "types/signature.hpp"
#include "types/signed_aggregated_attestation.hpp"
#include "types/signed_attestation.hpp"
#include "types/signed_block.hpp"
#include "types/state.hpp"
#include "types/validator.hpp"

namespace lean {
  template <typename T>
  struct SszTestJsonT {
    using Type = T;

    T value;
    qtils::ByteVec serialized;
    std::optional<std::string> expect_exception;

    JSON_FIELDS(value, serialized, expect_exception);
  };

  struct SszTestJson {
    struct Disabled : ssz::ssz_container {
      uint8_t nonce;
      SSZ_CONT(nonce);
    };

    template <typename T>
    struct Uint : ssz::ssz_variable_size_container {
      T v;

      SSZ_WRAPPER(v);
    };

    // xmss internals
    using Fp = Disabled;
    using HashTreeLayer = Disabled;
    using HashTreeOpening = Disabled;
    using PublicKey = Disabled;

    using AttestationSubnets = Disabled;
    using SyncCommitteeSubnets = Disabled;

    using DecodeBitlist8 = Disabled;
    using DecodeBitvector16 = Disabled;
    using SmokeBitlist8 = Disabled;
    using BoundaryBitvector7 = Disabled;
    using BoundaryBitlist256 = Disabled;
    using BoundaryBitvector255 = Disabled;
    using BoundaryUint64List32 = Disabled;
    using BoundaryBitvector256 = Disabled;
    using BoundaryBitvector9 = Disabled;
    using BoundaryBitvector257 = Disabled;
    using BoundaryBitvector1 = Disabled;
    using SampleBitlist16 = Disabled;
    using SampleBitvector64 = Disabled;
    using SampleBitvector8 = Disabled;
    using SampleBytes32List8 = Disabled;
    using SampleUint16Vector3 = Disabled;
    using SampleUint32List16 = Disabled;
    using SampleUint64Vector4 = Disabled;

    using ByteListMiB = ssz::list<uint8_t, (1 << 20)>;
    using Bytes32 = qtils::ByteArr<32>;
    using Bytes4 = qtils::ByteArr<4>;
    using Bytes52 = qtils::ByteArr<52>;
    using Bytes64 = qtils::ByteArr<64>;

    using Boolean = bool;
    using Uint8 = Uint<uint8_t>;
    using Uint16 = Uint<uint16_t>;
    using Uint32 = Uint<uint32_t>;
    using Uint64 = Uint<uint64_t>;

    std::variant<SszTestJsonT<AggregatedAttestation>,
                 SszTestJsonT<AggregatedSignatureProof>,
                 SszTestJsonT<Attestation>,
                 SszTestJsonT<AttestationData>,
                 SszTestJsonT<AttestationSubnets>,
                 SszTestJsonT<Block>,
                 SszTestJsonT<BlockBody>,
                 SszTestJsonT<BlockHeader>,
                 SszTestJsonT<BlocksByRootRequest>,
                 SszTestJsonT<BlockSignatures>,
                 SszTestJsonT<Boolean>,
                 SszTestJsonT<ByteListMiB>,
                 SszTestJsonT<Bytes32>,
                 SszTestJsonT<Bytes4>,
                 SszTestJsonT<Bytes52>,
                 SszTestJsonT<Bytes64>,
                 SszTestJsonT<Checkpoint>,
                 SszTestJsonT<Config>,
                 SszTestJsonT<DecodeBitlist8>,
                 SszTestJsonT<DecodeBitvector16>,
                 SszTestJsonT<SmokeBitlist8>,
                 SszTestJsonT<BoundaryBitvector7>,
                 SszTestJsonT<BoundaryBitlist256>,
                 SszTestJsonT<BoundaryBitvector255>,
                 SszTestJsonT<BoundaryUint64List32>,
                 SszTestJsonT<BoundaryBitvector256>,
                 SszTestJsonT<BoundaryBitvector9>,
                 SszTestJsonT<BoundaryBitvector257>,
                 SszTestJsonT<BoundaryBitvector1>,
                 SszTestJsonT<Fp>,
                 SszTestJsonT<HashTreeLayer>,
                 SszTestJsonT<HashTreeOpening>,
                 SszTestJsonT<PublicKey>,
                 SszTestJsonT<SampleBitlist16>,
                 SszTestJsonT<SampleBitvector64>,
                 SszTestJsonT<SampleBitvector8>,
                 SszTestJsonT<SampleBytes32List8>,
                 SszTestJsonT<SampleUint16Vector3>,
                 SszTestJsonT<SampleUint32List16>,
                 SszTestJsonT<SampleUint64Vector4>,
                 SszTestJsonT<Signature>,
                 SszTestJsonT<SignedAggregatedAttestation>,
                 SszTestJsonT<SignedAttestation>,
                 SszTestJsonT<SignedBlock>,
                 SszTestJsonT<State>,
                 SszTestJsonT<StatusMessage>,
                 SszTestJsonT<SyncCommitteeSubnets>,
                 SszTestJsonT<Uint8>,
                 SszTestJsonT<Uint16>,
                 SszTestJsonT<Uint32>,
                 SszTestJsonT<Uint64>,
                 SszTestJsonT<Validator>>
        v;

    JSON_DISCRIMINATOR(type_name,
                       "AggregatedAttestation",
                       "AggregatedSignatureProof",
                       "Attestation",
                       "AttestationData",
                       "AttestationSubnets",
                       "Block",
                       "BlockBody",
                       "BlockHeader",
                       "BlocksByRootRequest",
                       "BlockSignatures",
                       "Boolean",
                       "ByteListMiB",
                       "Bytes32",
                       "Bytes4",
                       "Bytes52",
                       "Bytes64",
                       "Checkpoint",
                       "Config",
                       "DecodeBitlist8",
                       "DecodeBitvector16",
                       "SmokeBitlist8",
                       "BoundaryBitvector7",
                       "BoundaryBitlist256",
                       "BoundaryBitvector255",
                       "BoundaryUint64List32",
                       "BoundaryBitvector256",
                       "BoundaryBitvector9",
                       "BoundaryBitvector257",
                       "BoundaryBitvector1",
                       "Fp",
                       "HashTreeLayer",
                       "HashTreeOpening",
                       "PublicKey",
                       "SampleBitlist16",
                       "SampleBitvector64",
                       "SampleBitvector8",
                       "SampleBytes32List8",
                       "SampleUint16Vector3",
                       "SampleUint32List16",
                       "SampleUint64Vector4",
                       "Signature",
                       "SignedAggregatedAttestation",
                       "SignedAttestation",
                       "SignedBlock",
                       "State",
                       "Status",
                       "SyncCommitteeSubnets",
                       "Uint8",
                       "Uint16",
                       "Uint32",
                       "Uint64",
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
  void decode(const JsonIn &json, SszTestJson::Disabled &v) {}

  template <typename T>
  void decode(const JsonIn &json, SszTestJson::Uint<T> &v) {
    std::string s{decodeStr(json)};
    v.v = overflow<T>(json, std::stoull(s));
  }
}  // namespace lean::json
