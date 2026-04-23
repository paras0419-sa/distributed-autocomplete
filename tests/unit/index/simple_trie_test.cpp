#include <gtest/gtest.h>
#include <algorithm>
#include <vector>
#include "autocomplete/index/SimpleTrie.h"

using autocomplete::index::EntryId;
using autocomplete::index::SimpleTrie;

namespace {
std::vector<EntryId> collect(const SimpleTrie& t, std::string_view prefix) {
  std::vector<EntryId> out;
  t.walk_prefix(prefix, [&](EntryId id) { out.push_back(id); return true; });
  std::sort(out.begin(), out.end());
  return out;
}
}  // namespace

TEST(SimpleTrieTest, ExactMatchReturnsEntry) {
  SimpleTrie t;
  t.insert("revenue", 7);
  EXPECT_EQ(collect(t, "revenue"), std::vector<EntryId>{7});
}

TEST(SimpleTrieTest, PrefixReturnsAllMatchingEntries) {
  SimpleTrie t;
  t.insert("revenue", 1);
  t.insert("revenues", 2);
  t.insert("revert", 3);
  t.insert("region", 4);

  EXPECT_EQ(collect(t, "reven"), (std::vector<EntryId>{1, 2}));
  EXPECT_EQ(collect(t, "re"),    (std::vector<EntryId>{1, 2, 3, 4}));
  EXPECT_EQ(collect(t, "zzz"),   std::vector<EntryId>{});
}

TEST(SimpleTrieTest, SameTokenMultipleEntryIds) {
  SimpleTrie t;
  t.insert("sales", 10);
  t.insert("sales", 11);
  EXPECT_EQ(collect(t, "sales"), (std::vector<EntryId>{10, 11}));
}

TEST(SimpleTrieTest, VisitorCanStopEarly) {
  SimpleTrie t;
  for (EntryId i = 0; i < 100; ++i) t.insert("a", i);

  int seen = 0;
  t.walk_prefix("a", [&](EntryId) { ++seen; return seen < 5; });
  EXPECT_EQ(seen, 5);
}

TEST(SimpleTrieTest, EmptyPrefixIsRejected) {
  SimpleTrie t;
  t.insert("x", 0);
  auto got = collect(t, "");
  EXPECT_TRUE(got.empty());  // empty prefix = no-op by contract
}
