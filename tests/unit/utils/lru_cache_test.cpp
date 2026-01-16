/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "utils/lru_cache.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <thread>

#include "tests/testutil/dummy_error.hpp"

using outcome::result;

// A value type with visible identity, so we can verify pointer reuse.
struct Val {
  int id = 0;

  friend bool operator==(const Val &a, const Val &b) {
    return a.id == b.id;
  }
};

// For testing non-Value ValueArg branch (put with convertible / constructible).
struct ValArg {
  int id = 0;
  operator Val() const {  // NOLINT(google-explicit-constructor)
    return Val{id};
  }
};

TEST(LruCacheTest, GetOnEmptyReturnsNullopt) {
  lean::LruCache<int, Val, false> cache{3};

  auto got = cache.get(10);
  EXPECT_FALSE(got.has_value());
}

TEST(LruCacheTest, PutThenGetReturnsValue) {
  lean::LruCache<int, Val, false> cache{3};

  auto sp = cache.put(1, Val{42});
  ASSERT_TRUE(sp);
  EXPECT_EQ(sp->id, 42);

  auto got = cache.get(1);
  ASSERT_TRUE(got.has_value());
  ASSERT_TRUE(got.value());
  EXPECT_EQ(got.value()->id, 42);
}

TEST(LruCacheTest, GetUpdatesRecencySoLruIsEvicted) {
  // Max size 2:
  // put A, put B, get A (A becomes MRU), put C => should evict B
  lean::LruCache<int, Val, false> cache{2};

  cache.put(1, Val{1});                   // A
  cache.put(2, Val{2});                   // B
  ASSERT_TRUE(cache.get(1).has_value());  // touch A
  cache.put(3, Val{3});                   // C, evict LRU (B)

  EXPECT_TRUE(cache.get(1).has_value());
  EXPECT_FALSE(cache.get(2).has_value());
  EXPECT_TRUE(cache.get(3).has_value());
}

TEST(LruCacheTest, PutDoesNotEvictIfSizeBelowCapacity) {
  lean::LruCache<int, Val, false> cache{3};

  cache.put(1, Val{1});
  cache.put(2, Val{2});

  EXPECT_TRUE(cache.get(1).has_value());
  EXPECT_TRUE(cache.get(2).has_value());
}

TEST(LruCacheTest, EraseRemovesKey) {
  lean::LruCache<int, Val, false> cache{3};

  cache.put(1, Val{10});
  ASSERT_TRUE(cache.get(1).has_value());

  cache.erase(1);
  EXPECT_FALSE(cache.get(1).has_value());
}

TEST(LruCacheTest, EraseNonExistingDoesNothing) {
  lean::LruCache<int, Val, false> cache{3};

  cache.put(1, Val{10});
  cache.erase(999);  // should not crash / affect

  EXPECT_TRUE(cache.get(1).has_value());
}

TEST(LruCacheTest, EraseIfRemovesMatchingEntries) {
  lean::LruCache<int, Val, false> cache{5};

  cache.put(1, Val{10});
  cache.put(2, Val{20});
  cache.put(3, Val{30});

  cache.erase_if([](const int &key, const Val &value) {
    (void)key;
    return value.id >= 20;
  });

  EXPECT_TRUE(cache.get(1).has_value());
  EXPECT_FALSE(cache.get(2).has_value());
  EXPECT_FALSE(cache.get(3).has_value());
}

TEST(LruCacheTest, GetElseReturnsCachedValueWithoutCallingFunc) {
  lean::LruCache<int, Val, false> cache{3};

  cache.put(7, Val{77});

  int calls = 0;
  auto res = cache.get_else(7, [&]() -> result<Val> {
    ++calls;
    return Val{999};
  });

  ASSERT_TRUE(res.has_value());
  EXPECT_EQ(res.value()->id, 77);
  EXPECT_EQ(calls, 0);
}

TEST(LruCacheTest, GetElseCallsFuncAndStoresOnSuccess) {
  lean::LruCache<int, Val, false> cache{3};

  int calls = 0;
  auto res1 = cache.get_else(7, [&]() -> result<Val> {
    ++calls;
    return Val{123};
  });

  ASSERT_TRUE(res1.has_value());
  EXPECT_EQ(res1.value()->id, 123);
  EXPECT_EQ(calls, 1);

  // Second call should come from cache.
  auto res2 = cache.get_else(7, [&]() -> result<Val> {
    ++calls;
    return Val{999};
  });

  ASSERT_TRUE(res2.has_value());
  EXPECT_EQ(res2.value()->id, 123);
  EXPECT_EQ(calls, 1);
}

TEST(LruCacheTest, GetElsePropagatesFailureAndDoesNotStore) {
  lean::LruCache<int, Val, false> cache{3};

  int calls = 0;
  auto res1 = cache.get_else(7, [&]() -> result<Val> {
    ++calls;
    return testutil::DummyError::ERROR;
  });

  EXPECT_FALSE(res1.has_value());
  EXPECT_EQ(calls, 1);

  // Since failure should not store, func must be called again.
  auto res2 = cache.get_else(7, [&]() -> result<Val> {
    ++calls;
    return testutil::DummyError::ERROR;
  });

  EXPECT_FALSE(res2.has_value());
  EXPECT_EQ(calls, 2);
}

TEST(LruCacheTest, PutReusesSharedPtrForEqualValues_ValueArgIsValue) {
  lean::LruCache<int, Val, false> cache{10};

  auto p1 = cache.put(1, Val{5});
  auto p2 = cache.put(2, Val{5});  // equal value => should reuse pointer

  ASSERT_TRUE(p1);
  ASSERT_TRUE(p2);
  EXPECT_EQ(p1.get(), p2.get());

  auto g1 = cache.get(1);
  auto g2 = cache.get(2);
  ASSERT_TRUE(g1.has_value());
  ASSERT_TRUE(g2.has_value());
  EXPECT_EQ(g1.value().get(), g2.value().get());
}

TEST(LruCacheTest, PutReusesSharedPtrForEqualValues_ValueArgIsConvertible) {
  lean::LruCache<int, Val, false> cache{10};

  auto p1 = cache.put(1, ValArg{7});  // non-Value branch
  auto p2 = cache.put(2, ValArg{7});

  ASSERT_TRUE(p1);
  ASSERT_TRUE(p2);
  EXPECT_EQ(p1.get(), p2.get());
}

TEST(LruCacheTest, WhenFullAndReusingPointerStillEvictsLru) {
  // This covers the special path:
  // if existing equal value found and cache_.size() >= kMaxSize -> erase(min)
  lean::LruCache<int, Val, false> cache{2};

  cache.put(1, Val{1});                   // tick 1
  cache.put(2, Val{2});                   // tick 2
  ASSERT_TRUE(cache.get(1).has_value());  // make key=2 the LRU

  // Put new key=3 with value equal to existing key=1's value -> pointer reuse
  // Cache is full => should evict LRU (key=2)
  auto p = cache.put(3, Val{1});
  (void)p;

  EXPECT_TRUE(cache.get(1).has_value());
  EXPECT_FALSE(cache.get(2).has_value());
  EXPECT_TRUE(cache.get(3).has_value());
}

TEST(LruCacheTest, PriorityOverflowCompressesAndCacheStillWorks) {
  // Use small PriorityType to force overflow quickly.
  using Cache = lean::LruCache<int, Val, false, uint8_t>;
  Cache cache{3};

  // Force several accesses to wrap ticks_.
  cache.put(1, Val{1});            // tick=1
  cache.put(2, Val{2});            // tick=2
  cache.put(3, Val{3});            // tick=3
  for (int i = 0; i < 260; ++i) {  // will overflow uint8_t at 256
    (void)cache.get(1);
  }

  // Must still be functional after overflow handling.
  EXPECT_TRUE(cache.get(1).has_value());
  EXPECT_TRUE(cache.get(2).has_value());
  EXPECT_TRUE(cache.get(3).has_value());

  // Check eviction after overflow path too.
  cache.put(4, Val{4});
  // One of {1,2,3} must be evicted (capacity 3).
  int present = 0;
  present += cache.get(1).has_value() ? 1 : 0;
  present += cache.get(2).has_value() ? 1 : 0;
  present += cache.get(3).has_value() ? 1 : 0;
  present += cache.get(4).has_value() ? 1 : 0;
  EXPECT_EQ(present, 3);
  EXPECT_TRUE(cache.get(4).has_value());
}

TEST(LruCacheTest, CapacityOneEvictsPrevious) {
  lean::LruCache<int, Val, false> cache{1};

  cache.put(1, Val{1});
  EXPECT_TRUE(cache.get(1).has_value());

  cache.put(2, Val{2});  // must evict key=1
  EXPECT_FALSE(cache.get(1).has_value());
  EXPECT_TRUE(cache.get(2).has_value());
}

TEST(LruCacheTest, PutSameKeyTwiceReturnsFirstInsertedOnGet) {
  lean::LruCache<int, Val, false> cache{10};

  auto p1 = cache.put(1, Val{111});
  auto p2 = cache.put(1, Val{222});
  ASSERT_TRUE(p1);
  ASSERT_TRUE(p2);

  auto got = cache.get(1);
  ASSERT_TRUE(got.has_value());
  ASSERT_TRUE(got.value());

  // Current implementation returns the first matching entry in cache_.
  EXPECT_EQ(got.value()->id, 111);
}

TEST(LruCacheTest, ThreadSafeBasicSmoke) {
  // Not a strict TSAN test, but at least exercises locking path.
  lean::LruCache<int, Val, true> cache{50};

  std::atomic<bool> start{false};

  auto worker = [&](int base) {
    while (!start.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }
    for (int i = 0; i < 500; ++i) {
      int k = base + (i % 25);
      cache.put(k, Val{k});
      (void)cache.get(k);
      if ((i % 50) == 0) {
        cache.erase(k);
      }
    }
  };

  std::thread t1(worker, 0);
  std::thread t2(worker, 1000);
  start.store(true, std::memory_order_release);
  t1.join();
  t2.join();

  // Sanity: cache remains usable.
  cache.put(9999, Val{9999});
  EXPECT_TRUE(cache.get(9999).has_value());
}