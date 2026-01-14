/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app/bootnodes.hpp"

#include <libp2p/multi/multiaddress.hpp>
#include <libp2p/peer/peer_id.hpp>

// Implementation file for bootnodes - currently header-only but
// keeping this file for future implementation needs
namespace lean::app {
  BootnodeInfo::BootnodeInfo(libp2p::multi::Multiaddress address,
                             libp2p::PeerId id,
                             bool is_aggregator)
      : address{std::move(address)},
        peer_id{std::move(id)},
        is_aggregator{is_aggregator} {}

  Bootnodes::Bootnodes(std::vector<BootnodeInfo> nodes)
      : nodes_(std::move(nodes)) {}

  const std::vector<BootnodeInfo> &Bootnodes::getBootnodes() const {
    return nodes_;
  }

  /**
   * @brief Add a bootnode
   */
  void Bootnodes::addBootnode(BootnodeInfo node) {
    nodes_.emplace_back(std::move(node));
  }

  /**
   * @brief Check if any bootnodes are configured
   */
  bool Bootnodes::empty() const {
    return nodes_.empty();
  }

  /**
   * @brief Get the number of configured bootnodes
   */
  size_t Bootnodes::size() const {
    return nodes_.size();
  }
}  // namespace lean::app
