
#pragma once

#include <chrono>

#include "types/block.hpp"
#include "types/signed_aggregated_attestation.hpp"
#include "types/signed_attestation.hpp"

namespace lean {
  inline std::string leanInteropTestLog(std::string_view type,
                                        std::string_view arg) {
    return fmt::format("[\"LEAN-INTEROP-TEST\", {}, \"{}\", {}]",
                       std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count(),
                       type,
                       arg);
  }

  inline std::string leanInteropTest(const AttestationData &v) {
    return fmt::format("[{}, {}, {}, {}, \"{:xx}\"]",
                       v.source.slot,
                       v.target.slot,
                       v.head.slot,
                       v.slot,
                       v.head.root);
  }

  inline std::string leanInteropTest(const SignedAttestation &v) {
    return fmt::format("[{}, {}]", v.validator_id, leanInteropTest(v.data));
  }

  inline std::string leanInteropTest(const AggregatedAttestation &v) {
    return fmt::format("[[{}], {}]",
                       fmt::join(v.aggregation_bits.iter(), ", "),
                       leanInteropTest(v.data));
  }

  inline std::string leanInteropTest(const SignedAggregatedAttestation &v) {
    return fmt::format("[[{}], {}]",
                       fmt::join(v.proof.participants.iter(), ", "),
                       leanInteropTest(v.data));
  }

  inline std::string leanInteropTest(const Block &v) {
    return fmt::format(
        "{{\"slot\": {}, \"hash\": \"{:xx}\", \"parent\": \"{:xx}\", "
        "\"proposer\": {}, \"aggregations\": [{}]}}",
        v.slot,
        v.hash(),
        v.parent_root,
        v.proposer_index,
        fmt::join(
            v.body.attestations
                | std::views::transform([](const AggregatedAttestation &v) {
                    return leanInteropTest(v);
                  }),
            ", "));
  }
}  // namespace lean
