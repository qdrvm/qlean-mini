/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <print>

#include <libp2p/multi/multiaddress.hpp>
#include <qlean/todo.hpp>
#include <sszpp/fork.hpp>
#include <sszpp/ssz++.hpp>

int main() {
  ssz::fork_t fork{
      .previous_version =
          {
              std::byte{1},
              std::byte{2},
              std::byte{3},
              std::byte{4},
          },
      .current_version =
          {
              std::byte{5},
              std::byte{6},
              std::byte{7},
              std::byte{8},
          },
      .epoch = 9,
  };
  std::println(
      "serialize(fork_t {{ previous_version = [{}, {}, {}, {}], current_version = [{}, {}, {}, {}], epoch = {} }}) = [{}]",
      (uint8_t)fork.previous_version[0],
      (uint8_t)fork.previous_version[1],
      (uint8_t)fork.previous_version[2],
      (uint8_t)fork.previous_version[3],
      (uint8_t)fork.current_version[0],
      (uint8_t)fork.current_version[1],
      (uint8_t)fork.current_version[2],
      (uint8_t)fork.current_version[3],
      fork.epoch,
      ssz::to_string(ssz::serialize(fork)));

  auto address =
      libp2p::multi::Multiaddress::create("/ip4/127.0.0.1/tcp/8080").value();
  std::println("Created multiaddress: {}", address);
}
