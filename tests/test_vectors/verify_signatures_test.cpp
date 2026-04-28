/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blockchain/fork_choice.hpp"
#include "crypto/xmss/xmss_provider_impl.hpp"
#include "mock/app/validator_keys_manifest_mock.hpp"
#include "mock/blockchain/block_storage_mock.hpp"
#include "mock/blockchain/block_tree_mock.hpp"
#include "mock/blockchain/validator_registry_mock.hpp"
#include "mock/metrics_mock.hpp"
#include "test_vectors.hpp"
#include "testutil/prepare_loggers.hpp"
#include "verify_signatures_test_json.hpp"

struct VerifySignaturesTest : FixtureTest<lean::VerifySignaturesTestJson> {};
FIXTURE_INSTANTIATE(VerifySignaturesTest, "verify_signatures");

TEST_P(VerifySignaturesTest, VerifySignatures) {
  auto &[name, fixture] = GetParam();
  std::println("RUN {}", name);
  auto logsys = testutil::prepareLoggers();
  lean::ValidatorRegistry::ValidatorIndices validator_indices{0};
  auto validator_registry = std::make_shared<lean::ValidatorRegistryMock>();
  EXPECT_CALL(*validator_registry, currentValidatorIndices())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::ReturnRef(validator_indices));
  auto block_storage = std::make_shared<lean::blockchain::BlockStorageMock>();
  EXPECT_CALL(*block_storage, getState(fixture.signed_block.block.parent_root))
      .WillOnce(testing::Return(fixture.anchor_state));
  lean::ForkChoiceStore store{
      {},
      logsys,
      std::make_shared<lean::metrics::MetricsMock>(),
      {},
      {},
      {},
      {},
      {},
      0,
      validator_registry,
      std::make_shared<lean::app::ValidatorKeysManifestMock>(),
      std::make_shared<lean::crypto::xmss::XmssProviderImpl>(),
      std::make_shared<lean::blockchain::BlockTreeMock>(),
      block_storage,
      false,
      1,
  };
  EXPECT_EQ(store.validateBlockSignatures(fixture.signed_block),
            not fixture.expect_exception.has_value());
}
