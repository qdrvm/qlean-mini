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
      return shadow ? enr::makeIp((10 << 24) + index) : enr::Ip{127, 0, 0, 1};
    }

    SamplePeer(size_t index, bool shadow)
        : libp2p::SamplePeer{
            index,
            enr::toString(makeIp(index, shadow)),
            samplePort(index),
            Secp256k1,
          },
          enr_ip{makeIp(index,shadow)},
          enr{enr::encode(
            keypair,
            enr_ip,
            port
          ).value()} {}

    enr::Ip enr_ip;
    std::string enr;
  };
}  // namespace lean
