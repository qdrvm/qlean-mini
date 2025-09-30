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
    
    BootnodeInfo(libp2p::multi::Multiaddress addr, libp2p::PeerId id)
        : address(std::move(addr)), peer_id(std::move(id)) {}
  };

  /**
   * @brief Container for bootstrap nodes configuration
   */
  class Bootnodes {
   public:
    Bootnodes() = default;
    explicit Bootnodes(std::vector<BootnodeInfo> nodes) 
        : nodes_(std::move(nodes)) {}

    /**
     * @brief Get all bootnode information
     */
    const std::vector<BootnodeInfo>& getBootnodes() const {
      return nodes_;
    }

    /**
     * @brief Add a bootnode
     */
    void addBootnode(BootnodeInfo node) {
      nodes_.emplace_back(std::move(node));
    }

    /**
     * @brief Check if any bootnodes are configured
     */
    bool empty() const {
      return nodes_.empty();
    }

    /**
     * @brief Get the number of configured bootnodes
     */
    size_t size() const {
      return nodes_.size();
    }

   private:
    std::vector<BootnodeInfo> nodes_;
  };

}  // namespace lean::app
