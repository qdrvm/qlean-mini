/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "app/bootnodes.hpp"
#include "serde/enr.hpp"
#include "tests/mock/app/chain_spec_mock.hpp"
#include "modules/networking/networking.hpp"

using ::testing::Return;
using ::testing::ReturnRef;

/**
 * Test fixture for bootnode dependency injection testing
 */
class BootnodeInjectionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create mock chain spec with bootnodes
    chain_spec_mock = std::make_shared<lean::app::ChainSpecMock>();
    
    // Create test bootnodes
    auto addr = libp2p::multi::Multiaddress::create("/ip4/127.0.0.1/tcp/30333");
    ASSERT_TRUE(addr.has_value());
    
    // Create a valid peer ID using a proper multihash
    std::vector<uint8_t> peer_id_bytes = {
        0x12, 0x20,  // multihash: sha256 (0x12), length 32 (0x20)
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
    };
    auto peer_id = libp2p::peer::PeerId::fromBytes(peer_id_bytes);
    ASSERT_TRUE(peer_id.has_value());
    
    lean::app::BootnodeInfo bootnode_info(std::move(addr.value()), std::move(peer_id.value()));
    std::vector<lean::app::BootnodeInfo> bootnode_list;
    bootnode_list.emplace_back(std::move(bootnode_info));
    
    test_bootnodes = lean::app::Bootnodes(std::move(bootnode_list));
  }

  std::shared_ptr<lean::app::ChainSpecMock> chain_spec_mock;
  lean::app::Bootnodes test_bootnodes;
};

/**
 * Test that Bootnodes class correctly stores and retrieves bootnode information
 */
TEST_F(BootnodeInjectionTest, BootnodesStoreAndRetrieve) {
  ASSERT_FALSE(test_bootnodes.empty());
  ASSERT_EQ(test_bootnodes.size(), 1);
  
  const auto& bootnode_list = test_bootnodes.getBootnodes();
  ASSERT_EQ(bootnode_list.size(), 1);
  
  const auto& bootnode = bootnode_list[0];
  EXPECT_EQ(bootnode.address.getStringAddress(), "/ip4/127.0.0.1/tcp/30333");
}

/**
 * Test that ChainSpec mock correctly returns bootnodes
 */
TEST_F(BootnodeInjectionTest, ChainSpecReturnsBootnodes) {
  EXPECT_CALL(*chain_spec_mock, getBootnodes())
      .WillOnce(ReturnRef(test_bootnodes));
  
  const auto& returned_bootnodes = chain_spec_mock->getBootnodes();
  EXPECT_FALSE(returned_bootnodes.empty());
  EXPECT_EQ(returned_bootnodes.size(), 1);
}

/**
 * Test empty bootnodes case
 */
TEST(BootnodeTest, EmptyBootnodes) {
  lean::app::Bootnodes empty_bootnodes;
  EXPECT_TRUE(empty_bootnodes.empty());
  EXPECT_EQ(empty_bootnodes.size(), 0);
  EXPECT_TRUE(empty_bootnodes.getBootnodes().empty());
}

/**
 * Test ENR parsing and bootnode creation
 */
TEST(BootnodeTest, ENRParsing) {
  // Valid ENR from the ENR test suite
  std::string test_enr = "enr:-IW4QHcpC4AgOv7WXk1V8E56DDAy6KJ09VMOxSTUwgOqkLF6YihJc5Eoeo4UX1bm9H39Xl-831fomuqR3TZzB3S2IPoBgmlkgnY0gmlwhH8AAAGEcXVpY4InEIlzZWNwMjU2azGhA21sqsJIr5b2r6f5BPVQJToPPvP1qi_mg4qVshZpFGji";
  
  // Test ENR parsing directly
  auto enr_result = lean::enr::decode(test_enr);
  ASSERT_TRUE(enr_result.has_value()) << "ENR parsing should succeed";
  
  const auto& enr = enr_result.value();
  auto peer_id = enr.peerId();
  auto connect_addr = enr.connectAddress();
  
  EXPECT_FALSE(peer_id.toBase58().empty());
  EXPECT_FALSE(connect_addr.getStringAddress().empty());
  
  // Create bootnode info from parsed ENR
  lean::app::BootnodeInfo bootnode_info(connect_addr, peer_id);
  std::vector<lean::app::BootnodeInfo> bootnode_list;
  bootnode_list.emplace_back(std::move(bootnode_info));
  
  lean::app::Bootnodes bootnodes(std::move(bootnode_list));
  EXPECT_FALSE(bootnodes.empty());
  EXPECT_EQ(bootnodes.size(), 1);
  
  const auto& retrieved_bootnodes = bootnodes.getBootnodes();
  EXPECT_EQ(retrieved_bootnodes.size(), 1);
  EXPECT_EQ(retrieved_bootnodes[0].peer_id, peer_id);
  EXPECT_EQ(retrieved_bootnodes[0].address.getStringAddress(), connect_addr.getStringAddress());
}