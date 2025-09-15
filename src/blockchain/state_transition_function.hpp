/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/enum_error_code.hpp>
#include <qtils/outcome.hpp>

#include "types/slot.hpp"

namespace lean {
  struct Block;
  struct BlockBody;
  struct Config;
  struct SignedBlock;
  struct SignedVote;
  struct State;

  class StateTransitionFunction {
   public:
    enum class Error {
      INVALID_SLOT,
      STATE_ROOT_DOESNT_MATCH,
      INVALID_PROPOSER,
      PARENT_ROOT_DOESNT_MATCH,
    };
    Q_ENUM_ERROR_CODE_FRIEND(Error) {
      using E = decltype(e);
      switch (e) {
        case E::INVALID_SLOT:
          return "Invalid slot";
        case E::INVALID_PROPOSER:
          return "Invalid proposer";
        case E::PARENT_ROOT_DOESNT_MATCH:
          return "Parent root doesn't match";
        case E::STATE_ROOT_DOESNT_MATCH:
          return "State root doesn't match";
      }
      abort();
    }

    State generateGenesisState(const Config &config) const;
    Block genesisBlock(const State &state) const;

    /**
     * Apply block to parent state.
     * @returns new state
     */
    outcome::result<State> stateTransition(const SignedBlock &signed_block,
                                           const State &parent_state,
                                           bool check_state_root) const;

   private:
    outcome::result<void> processSlots(State &state, Slot slot) const;
    void processSlot(State &state) const;
    outcome::result<void> processBlock(State &state, const Block &block) const;
    outcome::result<void> processBlockHeader(State &state,
                                             const Block &block) const;
    void processOperations(State &state, const BlockBody &body) const;
    void processAttestations(State &state,
                             const std::vector<SignedVote> &attestations) const;
  };
}  // namespace lean
