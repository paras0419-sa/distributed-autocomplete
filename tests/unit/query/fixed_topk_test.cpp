#include <gtest/gtest.h>
#include <cstdint>
#include "autocomplete/query/FixedTopK.h"

using autocomplete::query::FixedTopK;

struct Scored {
  std::uint32_t id;
  float         score;
};

struct ByScore {
  bool operator()(const Scored& a, const Scored& b) const {
    return a.score < b.score;  // min-heap of Scored → a < b ⇒ "a is smaller"
  }
};

TEST(FixedTopKTest, KeepsTopKByScoreDescending) {
  FixedTopK<Scored, ByScore> top{3};
  top.offer({1, 1.0f});
  top.offer({2, 5.0f});
  top.offer({3, 3.0f});
  top.offer({4, 2.0f});  // dropped (1.0 is already out)
  top.offer({5, 4.0f});

  auto sorted = top.take_sorted();
  ASSERT_EQ(sorted.size(), 3u);
  EXPECT_EQ(sorted[0].id, 2u);  // 5.0
  EXPECT_EQ(sorted[1].id, 5u);  // 4.0
  EXPECT_EQ(sorted[2].id, 3u);  // 3.0
}

TEST(FixedTopKTest, UnderCapacityReturnsAll) {
  FixedTopK<Scored, ByScore> top{5};
  top.offer({1, 1.0f});
  top.offer({2, 3.0f});
  auto s = top.take_sorted();
  ASSERT_EQ(s.size(), 2u);
  EXPECT_EQ(s[0].id, 2u);
}

TEST(FixedTopKTest, ZeroCapacityStoresNothing) {
  FixedTopK<Scored, ByScore> top{0};
  top.offer({1, 99.0f});
  EXPECT_TRUE(top.take_sorted().empty());
}
