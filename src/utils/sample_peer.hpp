/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/common/sample_peer.hpp>

#include "serde/enr.hpp"

namespace lean {
  struct SamplePeer : libp2p::SamplePeer {
    static enr::Ip makeIp(size_t index, bool shadow) {
      if (shadow) {
        return enr::makeIp("10.0.0.0", index);
      }
      return enr::makeIp("127.0.0.1", 0);
    }

    SamplePeer(size_t index,bool is_aggregator, bool shadow)
        : libp2p::SamplePeer{
            index,
            enr::toString(makeIp(index, shadow)),
            samplePort(index),
            Secp256k1,
          },
          enr_ip{makeIp(index,shadow)},
          is_aggregator{is_aggregator},
          enr{enr::encode(
            keypair,
            enr_ip,
            port,
            is_aggregator
          ).value()} {}

    enr::Ip enr_ip;
    bool is_aggregator;
    std::string enr;
  };
}  // namespace lean
