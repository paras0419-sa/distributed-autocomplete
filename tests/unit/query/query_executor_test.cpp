#include <gtest/gtest.h>
#include <memory>
#include <string>
#include "autocomplete/index/IndexView.h"
#include "autocomplete/index/SimpleTrie.h"
#include "autocomplete/memory/StringArena.h"
#include "autocomplete/query/QueryExecutor.h"

using namespace autocomplete;
using autocomplete::index::Entry;
using autocomplete::index::SourceFlag;

namespace {
std::shared_ptr<index::IndexView> build_fixture() {
  auto arena = std::make_shared<memory::StringArena>();
  auto table = std::make_shared<index::EntryTable>();
  auto trie  = std::make_shared<index::SimpleTrie>();

  auto add = [&](std::string_view phrase, float w) {
    auto h  = arena->append(phrase);
    auto id = table->add(Entry{h.offset, w, 0, h.length,
                               SourceFlag::kColumn, 0});
    // Single-token phrases in this fixture; multi-token tokenization
    // is exercised in a separate test.
    trie->insert(phrase, id);
  };

  add("revenue",   3.0f);
  add("revenues",  2.0f);
  add("revert",    1.0f);
  add("region",    5.0f);
  add("reactor",   0.5f);
  return std::make_shared<index::IndexView>(trie, table, arena, 1);
}
}  // namespace

TEST(QueryExecutorTest, TopKOrderedByWeightDesc) {
  auto view = build_fixture();
  query::QueryExecutor qe;

  std::string prefix = "re";
  auto res = qe.execute(*view, prefix, /*k=*/3);

  ASSERT_EQ(res.size(), 3u);
  EXPECT_EQ(res[0].phrase, "region");
  EXPECT_EQ(res[1].phrase, "revenue");
  EXPECT_EQ(res[2].phrase, "revenues");
}

TEST(QueryExecutorTest, ZeroResultsOnUnknownPrefix) {
  auto view = build_fixture();
  query::QueryExecutor qe;
  std::string prefix = "xyz";
  EXPECT_TRUE(qe.execute(*view, prefix, 10).empty());
}

TEST(QueryExecutorTest, MultiTokenPrefixUsesLastToken) {
  // Phase 1: multi-token intersect is deferred; we match on the final
  // token's prefix and ignore prior tokens. This test pins that contract.
  auto view = build_fixture();
  query::QueryExecutor qe;
  std::string prefix = "q4 reg";
  auto res = qe.execute(*view, prefix, 5);
  ASSERT_EQ(res.size(), 1u);
  EXPECT_EQ(res[0].phrase, "region");
}

TEST(QueryExecutorTest, CapacityBoundedByK) {
  auto view = build_fixture();
  query::QueryExecutor qe;
  std::string prefix = "re";
  auto res = qe.execute(*view, prefix, 2);
  EXPECT_EQ(res.size(), 2u);
}
