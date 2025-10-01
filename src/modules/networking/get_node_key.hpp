/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include <boost/algorithm/string/trim.hpp>
#include <libp2p/crypto/key.hpp>
#include <libp2p/crypto/random_generator/boost_generator.hpp>
#include <libp2p/crypto/secp256k1_provider/secp256k1_provider_impl.hpp>
#include <qtils/read_file.hpp>
#include <qtils/unhex.hpp>

namespace lean {
  namespace detail {
    inline libp2p::crypto::secp256k1::Secp256k1ProviderImpl &
    secp256k1Provider() {
      static libp2p::crypto::secp256k1::Secp256k1ProviderImpl provider{
          std::make_shared<libp2p::crypto::random::BoostRandomGenerator>()};
      return provider;
    }

    inline libp2p::crypto::KeyPair toKeyPair(
        const libp2p::crypto::secp256k1::KeyPair &keypair) {
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
  }  // namespace detail

  inline outcome::result<libp2p::crypto::KeyPair> keyPairFromPrivateKeyHex(
      std::string_view hex) {
    std::string trimmed{hex};
    boost::trim(trimmed);
    libp2p::crypto::secp256k1::KeyPair keypair;
    BOOST_OUTCOME_TRY(qtils::unhex0x(keypair.private_key, trimmed, true));
    BOOST_OUTCOME_TRY(keypair.public_key,
                      detail::secp256k1Provider().derive(keypair.private_key));
    return detail::toKeyPair(keypair);
  }

  inline libp2p::crypto::KeyPair randomKeyPair() {
    auto keypair = detail::secp256k1Provider().generate().value();
    return detail::toKeyPair(keypair);
  }

  inline outcome::result<libp2p::crypto::KeyPair> getNodeKey(
      const std::filesystem::path &path) {
    if (std::filesystem::exists(path)) {
      BOOST_OUTCOME_TRY(auto hex, qtils::readText(path));
      return keyPairFromPrivateKeyHex(hex);
    }
    BOOST_OUTCOME_TRY(auto generated, detail::secp256k1Provider().generate());
    return detail::toKeyPair(generated);
  }
}  // namespace lean
