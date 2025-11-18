/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "crypto/xmss/xmss_provider_impl.hpp"

#include <gtest/gtest.h>

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

  auto keypair = provider_->generateKeypair(activation_epoch, num_active_epochs);

  EXPECT_FALSE(keypair.public_key.empty());
  EXPECT_FALSE(keypair.private_key.empty());

  // Check reasonable sizes
  EXPECT_GT(keypair.public_key.size(), 0);
  EXPECT_GT(keypair.private_key.size(), 0);
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
  bool result = provider_->verify(keypair.public_key, message, signature);
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
  bool result = provider_->verify(keypair.public_key, wrong_message, signature);
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
  bool result = provider_->verify(keypair2.public_key, message, signature);
  EXPECT_FALSE(result);
}

TEST_F(XmssProviderTest, SignatureIncludesEpoch) {
  auto keypair = provider_->generateKeypair(0, 10000);
  qtils::ByteVec message(32, 0x42);

  uint32_t epoch1 = 100;
  uint32_t epoch2 = 200;

  auto sig1 = provider_->sign(keypair.private_key, epoch1, message);
  auto sig2 = provider_->sign(keypair.private_key, epoch2, message);

  // Signatures should be different (different epochs)
  EXPECT_NE(sig1, sig2);

  // Extract epochs from signatures
  ASSERT_GE(sig1.size(), 4);
  ASSERT_GE(sig2.size(), 4);

  uint32_t extracted_epoch1 = static_cast<uint32_t>(sig1[0])
                             | (static_cast<uint32_t>(sig1[1]) << 8)
                             | (static_cast<uint32_t>(sig1[2]) << 16)
                             | (static_cast<uint32_t>(sig1[3]) << 24);

  uint32_t extracted_epoch2 = static_cast<uint32_t>(sig2[0])
                             | (static_cast<uint32_t>(sig2[1]) << 8)
                             | (static_cast<uint32_t>(sig2[2]) << 16)
                             | (static_cast<uint32_t>(sig2[3]) << 24);

  EXPECT_EQ(extracted_epoch1, epoch1);
  EXPECT_EQ(extracted_epoch2, epoch2);
}

