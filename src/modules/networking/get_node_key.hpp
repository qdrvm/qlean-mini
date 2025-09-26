/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <boost/algorithm/string/trim.hpp>
#include <libp2p/crypto/key.hpp>
#include <libp2p/crypto/random_generator/boost_generator.hpp>
#include <libp2p/crypto/secp256k1_provider/secp256k1_provider_impl.hpp>
#include <qtils/read_file.hpp>
#include <qtils/unhex.hpp>

namespace lean {
  /**
   * Read existing node key or generate new.
   */
  inline outcome::result<libp2p::crypto::KeyPair> getNodeKey(
      const std::filesystem::path &path) {
    static libp2p::crypto::secp256k1::Secp256k1ProviderImpl secp256k1{
        std::make_shared<libp2p::crypto::random::BoostRandomGenerator>()};
    libp2p::crypto::secp256k1::KeyPair keypair;
    if (std::filesystem::exists(path)) {
      BOOST_OUTCOME_TRY(auto hex, qtils::readText(path));
      boost::trim(hex);
      BOOST_OUTCOME_TRY(qtils::unhex0x(keypair.private_key, hex));
      BOOST_OUTCOME_TRY(keypair.public_key,
                        secp256k1.derive(keypair.private_key));
    } else {
      BOOST_OUTCOME_TRY(keypair, secp256k1.generate());
    }
    return libp2p::crypto::KeyPair{
        .publicKey = {{
            .type = libp2p::crypto::Key::Type::Secp256k1,
            .data = qtils::ByteVec{keypair.public_key},
        }},
        .privateKey = {{
            .type = libp2p::crypto::Key::Type::Secp256k1,
            .data = qtils::ByteVec{keypair.private_key},
        }},
    };
  }
}  // namespace lean
