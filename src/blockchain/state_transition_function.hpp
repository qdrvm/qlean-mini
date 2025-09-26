/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <qtils/enum_error_code.hpp>
#include <qtils/outcome.hpp>

#include "app/impl/chain_spec_impl.hpp"
#include "types/slot.hpp"

namespace lean {
  struct Block;
  struct BlockBody;
  struct Config;
  struct SignedBlock;
  struct SignedVote;
  struct State;

  class STF {
   public:
    enum class Error {
      INVALID_SLOT,
      STATE_ROOT_DOESNT_MATCH,
      INVALID_PROPOSER,
      PARENT_ROOT_DOESNT_MATCH,
      INVALID_VOTE_SOURCE_SLOT,
      INVALID_VOTE_TARGET_SLOT,
      INVALID_VOTER,
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
        case E::INVALID_VOTE_SOURCE_SLOT:
          return "Invalid vote source slot";
        case E::INVALID_VOTE_TARGET_SLOT:
          return "Invalid vote target slot";
        case E::INVALID_VOTER:
          return "Invalid voter";
      }
      abort();
    }

    static State generateGenesisState(const Config &config);
    static Block genesisBlock(const State &state);

    /**
     * Apply block to parent state.
     * @returns new state
     */
    outcome::result<State> stateTransition(const SignedBlock &signed_block,
                                           const State &parent_state,
                                           bool check_state_root) const;

    outcome::result<void> processSlots(State &state, Slot slot) const;
    outcome::result<void> processBlock(State &state, const Block &block) const;

   private:
    void processSlot(State &state) const;
    outcome::result<void> processBlockHeader(State &state,
                                             const Block &block) const;
    outcome::result<void> processOperations(State &state,
                                            const BlockBody &body) const;
    outcome::result<void> processAttestations(
        State &state, const std::vector<SignedVote> &attestations) const;
    bool validateProposerIndex(const State &state, const Block &block) const;
  };
}  // namespace lean
