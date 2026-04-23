#include <gtest/gtest.h>
#include <memory>
#include "autocomplete/index/IndexView.h"
#include "autocomplete/index/SimpleTrie.h"
#include "autocomplete/memory/StringArena.h"

using autocomplete::index::Entry;
using autocomplete::index::IndexView;
using autocomplete::index::SimpleTrie;
using autocomplete::index::SourceFlag;
using autocomplete::memory::StringArena;

TEST(IndexViewTest, BundlesAllThreeLayersAndVersion) {
  auto arena = std::make_shared<StringArena>();
  auto table = std::make_shared<autocomplete::index::EntryTable>();
  auto trie  = std::make_shared<SimpleTrie>();

  auto h  = arena->append("total");
  auto id = table->add(Entry{h.offset, 1.0f, 0, h.length,
                             SourceFlag::kColumn, 0});
  trie->insert("total", id);

  IndexView view{trie, table, arena, /*version=*/42};

  EXPECT_EQ(view.version(), 42u);
  EXPECT_EQ(view.entries().size(), 1u);
  EXPECT_EQ(view.arena().view({h.offset, h.length}), "total");
  EXPECT_EQ(view.index().token_count(), 1u);
}
