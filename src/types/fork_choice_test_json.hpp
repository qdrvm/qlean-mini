/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "serde/json_fwd.hpp"
#include "types/block.hpp"
#include "types/block_with_attestation.hpp"
#include "types/signed_attestation.hpp"
#include "types/state.hpp"

namespace lean {
  enum class AttestationCheckLocation {
    NEW,
    KNOWN,
  };
  JSON_ENUM(AttestationCheckLocation,
            {AttestationCheckLocation::NEW, "new"},
            {AttestationCheckLocation::KNOWN, "known"});

  struct AttestationCheck {
    ValidatorIndex validator;
    Slot attestation_slot;
    std::optional<Slot> head_slot;
    std::optional<Slot> source_slot;
    std::optional<Slot> target_slot;
    AttestationCheckLocation location;

    JSON_CAMEL(validator,
               attestation_slot,
               head_slot,
               source_slot,
               target_slot,
               location);
  };

  struct StoreChecks {
    std::optional<Interval> time;
    std::optional<Slot> head_slot;
    std::optional<BlockHash> head_root;
    std::optional<Slot> latest_justified_slot;
    std::optional<BlockHash> latest_justified_root;
    std::optional<Slot> latest_finalized_slot;
    std::optional<BlockHash> latest_finalized_root;
    std::optional<BlockHash> safe_target;
    std::optional<Slot> attestation_target_slot;
    std::optional<std::vector<AttestationCheck>> attestation_checks;

    JSON_CAMEL(time,
               head_slot,
               head_root,
               latest_justified_slot,
               latest_justified_root,
               latest_finalized_slot,
               latest_finalized_root,
               safe_target,
               attestation_target_slot,
               attestation_checks);
  };

  struct BaseForkChoiceStep {
    bool valid;
    std::optional<StoreChecks> checks;
  };

  struct TickStep : BaseForkChoiceStep {
    TimestampSeconds time;

    JSON_CAMEL(valid, checks, time);
  };

  struct BlockStep : BaseForkChoiceStep {
    BlockWithAttestation block;

    JSON_CAMEL(valid, checks, block);
  };

  struct AttestationStep : BaseForkChoiceStep {
    SignedAttestation attestation;

    JSON_CAMEL(valid, checks, attestation);
  };

  struct ForkChoiceStep {
    std::variant<TickStep, BlockStep, AttestationStep> v;

    JSON_CAMEL_DISCRIMINATOR("step_type", "tick", "block", "attestation");
  };

  struct ForkChoiceTestJson {
    State anchor_state;
    Block anchor_block;
    std::vector<ForkChoiceStep> steps;
    Slot max_slot;

    JSON_CAMEL(anchor_state, anchor_block, steps, max_slot);
  };
}  // namespace lean
