/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "crypto/xmss/xmss_provider_impl.hpp"

using namespace lean::crypto::xmss;

uint64_t activation_epoch = 0;
uint64_t num_active_epochs = 10;
uint32_t epoch = 5;
uint32_t wrong_epoch = 6;

XmssMessage message{0x42};
XmssMessage wrong_message{0x43};

class XmssProviderTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    provider_ = std::make_unique<XmssProviderImpl>();
    keypair = provider_->generateKeypair(activation_epoch, num_active_epochs);
    keypair2 = provider_->generateKeypair(activation_epoch, num_active_epochs);
  }

  static std::unique_ptr<XmssProviderImpl> provider_;
  static XmssKeypair keypair;
  static XmssKeypair keypair2;
};

decltype(XmssProviderTest::provider_) XmssProviderTest::provider_;
decltype(XmssProviderTest::keypair) XmssProviderTest::keypair;
decltype(XmssProviderTest::keypair2) XmssProviderTest::keypair2;

TEST_F(XmssProviderTest, GenerateKeypair) {
  EXPECT_FALSE(keypair.public_key.empty());
  EXPECT_NE(keypair.private_key, nullptr);

  // Check reasonable sizes
  EXPECT_GT(keypair.public_key.size(), 0);
}

TEST_F(XmssProviderTest, SignAndVerify) {
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
  // Create and sign a message
  auto signature = provider_->sign(keypair.private_key, epoch, message);

  // Verification should fail
  bool result =
      provider_->verify(keypair.public_key, wrong_message, epoch, signature);
  EXPECT_FALSE(result);
}

TEST_F(XmssProviderTest, VerifyFailsWithWrongPublicKey) {
  // Sign with first keypair
  auto signature = provider_->sign(keypair.private_key, epoch, message);

  // Try to verify with second keypair's public key
  bool result =
      provider_->verify(keypair2.public_key, message, epoch, signature);
  EXPECT_FALSE(result);
}

TEST_F(XmssProviderTest, VerifyFailsWithWrongEpoch) {
  // Create and sign a message with a specific epoch
  auto signature = provider_->sign(keypair.private_key, epoch, message);

  // Attempt to verify using a different epoch
  bool result =
      provider_->verify(keypair.public_key, message, wrong_epoch, signature);
  EXPECT_FALSE(result);
}

TEST_F(XmssProviderTest, AggregateSignatures) {
  std::vector<XmssPublicKey> public_keys{
      keypair.public_key,
      keypair2.public_key,
  };
  std::vector<XmssSignature> signatures{
      provider_->sign(keypair.private_key, epoch, message),
      provider_->sign(keypair2.private_key, epoch, message),
  };
  auto aggregated_signature =
      provider_->aggregateSignatures(public_keys, signatures, epoch, message);
  EXPECT_TRUE(provider_->verifyAggregatedSignatures(
      public_keys, epoch, message, aggregated_signature));
}
