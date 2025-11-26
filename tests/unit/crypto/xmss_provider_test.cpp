/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "crypto/xmss/xmss_provider_impl.hpp"

using namespace lean::crypto::xmss;

class XmssProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    provider_ = std::make_unique<XmssProviderImpl>();
  }

  std::unique_ptr<XmssProviderImpl> provider_;
};

TEST_F(XmssProviderTest, GenerateKeypair) {
  uint64_t activation_epoch = 0;
  uint64_t num_active_epochs = 10000;

  auto keypair =
      provider_->generateKeypair(activation_epoch, num_active_epochs);

  EXPECT_FALSE(keypair.public_key.empty());
  EXPECT_NE(keypair.private_key, nullptr);

  // Check reasonable sizes
  EXPECT_GT(keypair.public_key.size(), 0);
}

TEST_F(XmssProviderTest, SignAndVerify) {
  // Generate keypair
  auto keypair = provider_->generateKeypair(0, 10000);

  // Create a test message (32 bytes as required by XMSS)
  qtils::ByteVec message(32, 0x42);
  uint32_t epoch = 100;

  // Sign the message
  auto signature = provider_->sign(keypair.private_key, epoch, message);

  EXPECT_FALSE(signature.empty());
  EXPECT_GE(signature.size(), 4);  // At least epoch prefix

  // Verify the signature
  bool result =
      provider_->verify(keypair.public_key, message, epoch, signature);
  EXPECT_TRUE(result);
}

TEST_F(XmssProviderTest, VerifyFailsWithWrongMessage) {
  // Generate keypair
  auto keypair = provider_->generateKeypair(0, 10000);

  // Create and sign a message
  qtils::ByteVec message(32, 0x42);
  uint32_t epoch = 100;
  auto signature = provider_->sign(keypair.private_key, epoch, message);

  // Modify the message
  qtils::ByteVec wrong_message(32, 0x43);

  // Verification should fail
  bool result =
      provider_->verify(keypair.public_key, wrong_message, epoch, signature);
  EXPECT_FALSE(result);
}

TEST_F(XmssProviderTest, VerifyFailsWithWrongPublicKey) {
  // Generate two keypairs
  auto keypair1 = provider_->generateKeypair(0, 10000);
  auto keypair2 = provider_->generateKeypair(0, 10000);

  // Sign with first keypair
  qtils::ByteVec message(32, 0x42);
  uint32_t epoch = 100;
  auto signature = provider_->sign(keypair1.private_key, epoch, message);

  // Try to verify with second keypair's public key
  bool result =
      provider_->verify(keypair2.public_key, message, epoch, signature);
  EXPECT_FALSE(result);
}

TEST_F(XmssProviderTest, VerifyFailsWithWrongEpoch) {
  // Generate keypair
  auto keypair = provider_->generateKeypair(0, 10000);

  // Create and sign a message with a specific epoch
  qtils::ByteVec message(32, 0x42);
  uint32_t signing_epoch = 100;
  auto signature = provider_->sign(keypair.private_key, signing_epoch, message);

  // Attempt to verify using a different epoch
  uint32_t wrong_epoch = 101;
  bool result =
      provider_->verify(keypair.public_key, message, wrong_epoch, signature);
  EXPECT_FALSE(result);
}
