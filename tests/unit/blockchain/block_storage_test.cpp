/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <qtils/bytes.hpp>
#include <qtils/test/outcome.hpp>

#include "blockchain/block_storage_error.hpp"
#include "blockchain/impl/anchor_block_impl.hpp"
#include "blockchain/impl/anchor_state_impl.hpp"
#include "blockchain/impl/block_storage_impl.hpp"
#include "blockchain/impl/block_storage_initializer.hpp"
#include "mock/app/chain_spec_mock.hpp"
#include "mock/crypto/hasher_mock.hpp"
#include "mock/storage/generic_storage_mock.hpp"
#include "mock/storage/spaced_storage_mock.hpp"
#include "qtils/error_throw.hpp"
#include "sszpp/ssz++.hpp"
#include "storage/storage_error.hpp"
#include "testutil/literals.hpp"
#include "testutil/prepare_loggers.hpp"
#include "types/block_data.hpp"

using lean::AnchorBlock;
using lean::AnchorState;
using lean::Block;
using lean::BlockBody;
using lean::BlockData;
using lean::BlockHash;
using lean::BlockHeader;
using lean::decode;
using lean::encode;
using lean::app::ChainSpecMock;
using lean::blockchain::BlockStorageError;
using lean::blockchain::BlockStorageImpl;
using lean::blockchain::BlockStorageInitializer;
using lean::crypto::HasherMock;
using lean::storage::BufferStorageMock;
using lean::storage::Space;
using lean::storage::SpacedStorageMock;
using qtils::ByteVec;
using qtils::ByteView;
using testing::_;
using testing::Ref;
using testing::Return;

class BlockStorageTest : public testing::Test {
 public:
  static void SetUpTestCase() {
    testutil::prepareLoggers();
  }

  void SetUp() override {
    genesis_block_hash = anchor_block->hash();

    hasher = std::make_shared<HasherMock>();
    spaced_storage = std::make_shared<SpacedStorageMock>();

    std::set<Space> required_spaces = {
        Space::Default,
        Space::SlotToHashes,
        Space::Header,
        Space::Body,
        Space::State,
        Space::Attestation,
        Space::Signature,
    };

    for (auto space : required_spaces) {
      auto storage = std::make_shared<BufferStorageMock>();
      spaces[space] = storage;
      EXPECT_CALL(*spaced_storage, getSpace(space))
          .WillRepeatedly(Return(storage));

      EXPECT_CALL(*storage, put(_, _))
          .WillRepeatedly(Return(outcome::success()));
      EXPECT_CALL(*storage, tryGetMock(_)).WillRepeatedly(Return(std::nullopt));
    }
  }

  BlockHash regular_block_hash{"regular"_arr32};
  BlockHash unhappy_block_hash{"unhappy"_arr32};

  qtils::SharedRef<lean::log::LoggingSystem> logsys =
      testutil::prepareLoggers();

  qtils::SharedRef<AnchorState> anchor_state =
      std::make_shared<lean::blockchain::AnchorStateImpl>(lean::State{});
  qtils::SharedRef<AnchorBlock> anchor_block =
      std::make_shared<lean::blockchain::AnchorBlockImpl>(*anchor_state);

  BlockHash genesis_block_hash;  // {"genesis"_arr32};

  qtils::SharedRef<ChainSpecMock> chain_spec =
      std::make_shared<ChainSpecMock>();
  qtils::SharedRef<HasherMock> hasher = std::make_shared<HasherMock>();
  qtils::SharedRef<SpacedStorageMock> spaced_storage =
      std::make_shared<SpacedStorageMock>();
  std::map<Space, std::shared_ptr<BufferStorageMock>> spaces;

  qtils::SharedRef<BlockStorageImpl> createWithGenesis() {
    // calculate hash of genesis block at put block header
    static auto encoded_header = ByteVec(encode(BlockHeader{}).value());
    ON_CALL(*hasher, sha2_256(encoded_header.view()))
        .WillByDefault(Return(genesis_block_hash));

    auto new_block_storage = std::make_shared<BlockStorageImpl>(
        logsys, spaced_storage, hasher, nullptr);

    return new_block_storage;
  }
};

/**
 * @given a hasher instance, a genesis block, and an empty map storage
 * @when initialising a block storage from it
 * @then initialisation will successful
 */
TEST_F(BlockStorageTest, CreateWithGenesis) {
  createWithGenesis();
}

/**
 * @given a hasher instance and an empty map storage
 * @when trying to initialise a block storage from it and storage throws an
 * error
 * @then storage will be initialized by genesis block
 */
TEST_F(BlockStorageTest, CreateWithEmptyStorage) {
  auto empty_storage = std::make_shared<BufferStorageMock>();

  // check if storage contained genesis block
  EXPECT_CALL(*empty_storage, tryGetMock(_))
      .WillRepeatedly(Return(std::nullopt));

  // put genesis block into storage
  EXPECT_CALL(*empty_storage, put(_, _))
      .WillRepeatedly(Return(outcome::success()));

  ASSERT_NO_THROW(BlockStorageImpl x(logsys, spaced_storage, hasher, {}));
}

/**
 * @given a hasher instance, a genesis block, and an map storage containing the
 * block
 * @when initialising a block storage from it
 * @then initialisation will fail because the genesis block is already at the
 * underlying storage (which is actually supposed to be empty)
 */
TEST_F(BlockStorageTest, CreateWithExistingGenesis) {
  // trying to get header of genesis block
  EXPECT_CALL(*spaces[Space::Header], contains(ByteView{genesis_block_hash}))
      .WillOnce(Return(outcome::success(true)));

  // Init underlying storage
  ASSERT_NO_THROW(BlockStorageInitializer(
      logsys, spaced_storage, anchor_block, anchor_state, chain_spec, hasher));

  // Create block storage
  ASSERT_NO_THROW(BlockStorageImpl(logsys, spaced_storage, hasher, {}));
}

/**
 * @given a hasher instance, a genesis block, and an map storage containing the
 * block
 * @when initialising a block storage from it and storage throws an error
 * @then initialisation will fail
 */
TEST_F(BlockStorageTest, CreateWithStorageError) {
  // trying to get header of genesis block
  EXPECT_CALL(*spaces[Space::Header], contains(ByteView{genesis_block_hash}))
      .WillOnce(Return(lean::storage::StorageError::IO_ERROR));

  // Init underlying storage
  EXPECT_THROW_OUTCOME(BlockStorageInitializer(logsys,
                                               spaced_storage,
                                               anchor_block,
                                               anchor_state,
                                               chain_spec,
                                               hasher),
                       lean::storage::StorageError::IO_ERROR);
}

/**
 * @given a block storage and a block that is not in storage yet
 * @when putting a block in the storage
 * @then block is successfully put
 */
TEST_F(BlockStorageTest, PutBlock) {
  auto block_storage = createWithGenesis();

  BlockData block;
  block.header.emplace();
  block.header->slot = 1;
  block.header->parent_root = genesis_block_hash;

  ASSERT_OUTCOME_SUCCESS(block_storage->putBlock(block));
}

/*
 * @given a block storage and a block that is not in storage yet
 * @when trying to get a block from the storage
 * @then an error is returned
 */
TEST_F(BlockStorageTest, GetBlockNotFound) {
  std::shared_ptr<BlockStorageImpl> block_storage;
  ASSERT_NO_THROW(block_storage = std::make_shared<BlockStorageImpl>(
                      logsys, spaced_storage, hasher, nullptr));

  EXPECT_OUTCOME_ERROR(get_res,
                       block_storage->getBlockHeader(genesis_block_hash),
                       BlockStorageError::HEADER_NOT_FOUND);
}

/*
 * @given a block storage and a block that is not in storage yet
 * @when trying to get a block from the storage
 * @then success value containing nullopt is returned
 */
TEST_F(BlockStorageTest, TryGetBlockNotFound) {
  std::shared_ptr<BlockStorageImpl> block_storage;
  ASSERT_NO_THROW(block_storage = std::make_shared<BlockStorageImpl>(
                      logsys, spaced_storage, hasher, nullptr));

  ASSERT_OUTCOME_SUCCESS(try_get_res,
                         block_storage->tryGetBlockHeader(genesis_block_hash));
  ASSERT_FALSE(try_get_res.has_value());
}

/**
 * @given a block storage and a block that is not in storage yet
 * @when putting a block in the storage and underlying storage throws an
 * error
 * @then block is not put and error is returned
 */
TEST_F(BlockStorageTest, PutWithStorageError) {
  auto block_storage = createWithGenesis();

  BlockBody body;

  BlockHeader header;
  header.slot = 1;
  header.parent_root = genesis_block_hash;
  header.body_root = lean::sszHash(body);

  BlockData block;
  block.header.emplace(header);
  block.header->slot = 1;
  block.header->parent_root = genesis_block_hash;
  block.body.emplace(body);

  auto encoded_header = ByteVec(encode(*block.header).value());
  ON_CALL(*hasher, sha2_256(encoded_header.view()))
      .WillByDefault(Return(regular_block_hash));

  ByteVec key{lean::sszHash(header)};

  EXPECT_CALL(*spaces[Space::Body], put(key.view(), _))
      .WillOnce(Return(lean::storage::StorageError::IO_ERROR));

  ASSERT_OUTCOME_ERROR(block_storage->putBlock(block),
                       lean::storage::StorageError::IO_ERROR);
}

/**
 * @given a block storage
 * @when removing a block from it
 * @then block is successfully removed if no error occurs in the underlying
 * storage, an error is returned otherwise
 */
TEST_F(BlockStorageTest, Remove) {
  auto block_storage = createWithGenesis();

  BlockData block;
  block.header.emplace();
  block.header->slot = 1;
  block.header->parent_root = genesis_block_hash;
  block.body.emplace();
  block.header->body_root = lean::sszHash(*block.body);

  ASSERT_OUTCOME_SUCCESS(hash, block_storage->putBlock(block));

  ByteVec encoded_header{encode(block.header.value()).value()};

  ASSERT_EQ(hash, block.header->hash());

  EXPECT_CALL(*spaces[Space::Header], tryGetMock(ByteView{hash}))
      .WillOnce(Return(encoded_header));
  EXPECT_CALL(*spaces[Space::Body], remove(ByteView{hash}))
      .WillOnce(Return(outcome::success()));
  EXPECT_CALL(*spaces[Space::Header], remove(ByteView{hash}))
      .WillOnce(Return(outcome::success()));
  EXPECT_CALL(*spaces[Space::Attestation], remove(ByteView{hash}))
      .WillOnce(Return(outcome::success()));
  EXPECT_CALL(*spaces[Space::State], remove(ByteView{hash}))
      .WillOnce(Return(outcome::success()));
  EXPECT_CALL(*spaces[Space::Signature], remove(ByteView{hash}))
      .WillOnce(Return(outcome::success()));

  ASSERT_OUTCOME_SUCCESS(block_storage->removeBlock(hash));

  EXPECT_CALL(*spaces[Space::Header], tryGetMock(ByteView{hash}))
      .WillOnce(Return(std::nullopt));

  ASSERT_OUTCOME_SUCCESS(block_storage->removeBlock(hash));
}
