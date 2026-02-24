/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>
#include <vector>

#include <libp2p/multi/multiaddress.hpp>
#include <libp2p/peer/peer_id.hpp>

namespace lean::app {

  /**
   * @brief Represents a single bootnode with its address and peer ID
   */
  struct BootnodeInfo {
    libp2p::multi::Multiaddress address;
    libp2p::PeerId peer_id;
    bool is_aggregator;

    BootnodeInfo(libp2p::multi::Multiaddress address,
                 libp2p::PeerId id,
                 bool is_aggregator);
  };

  /**
   * @brief Container for bootstrap nodes configuration
   */
  class Bootnodes {
   public:
    Bootnodes() = default;
    explicit Bootnodes(std::vector<BootnodeInfo> nodes);

    /**
     * @brief Get all bootnode information
     */
    const std::vector<BootnodeInfo> &getBootnodes() const;

    /**
     * @brief Add a bootnode
     */
    void addBootnode(BootnodeInfo node);

    /**
     * @brief Check if any bootnodes are configured
     */
    bool empty() const;

    /**
     * @brief Get the number of configured bootnodes
     */
    size_t size() const;

   private:
    std::vector<BootnodeInfo> nodes_;
  };

}  // namespace lean::app
