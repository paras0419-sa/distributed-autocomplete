#include <gtest/gtest.h>
#include "autocomplete/index/EntryTable.h"
#include "autocomplete/memory/StringArena.h"

using autocomplete::index::Entry;
using autocomplete::index::EntryTable;
using autocomplete::index::SourceFlag;
using autocomplete::memory::StringArena;

TEST(EntryTableTest, AddAndLookupByIdReturnsSameEntry) {
  StringArena arena;
  EntryTable t;
  auto ph = arena.append("revenue");

  auto id = t.add(Entry{ph.offset, 3.5f, /*last_seen*/ 100, ph.length,
                        SourceFlag::kColumn, 0});

  const Entry& e = t.at(id);
  EXPECT_EQ(e.weight, 3.5f);
  EXPECT_EQ(arena.view({e.phrase_offset, e.phrase_length}), "revenue");
  EXPECT_EQ(e.source_flag, SourceFlag::kColumn);
}

TEST(EntryTableTest, IdsAreMonotonic) {
  EntryTable t;
  auto a = t.add(Entry{});
  auto b = t.add(Entry{});
  EXPECT_LT(a, b);
  EXPECT_EQ(t.size(), 2u);
}

TEST(EntryTableTest, EntryIsOneCacheLine) {
  static_assert(sizeof(Entry) == 16, "Entry must stay cache-line friendly");
  SUCCEED();
}
