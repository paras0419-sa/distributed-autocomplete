#include <fstream>
#include <memory>
#include <string>
#include <gtest/gtest.h>
#include "autocomplete/index/EntryTable.h"
#include "autocomplete/index/IndexView.h"
#include "autocomplete/index/SimpleTrie.h"
#include "autocomplete/memory/StringArena.h"
#include "autocomplete/query/QueryExecutor.h"
#include "autocomplete/query/Tokenizer.h"

using namespace autocomplete;

namespace {
std::shared_ptr<index::IndexView> load(const std::string& path) {
  auto arena = std::make_shared<memory::StringArena>();
  auto table = std::make_shared<index::EntryTable>();
  auto trie  = std::make_shared<index::SimpleTrie>();
  query::Tokenizer tok;

  std::ifstream f(path);
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty()) continue;
    auto comma = line.find_last_of(',');
    auto phrase = line.substr(0, comma);
    float w = std::stof(line.substr(comma + 1));
    auto h  = arena->append(phrase);
    auto id = table->add(index::Entry{h.offset, w, 0, h.length,
                                      index::SourceFlag::kColumn, 0});
    std::string norm = phrase;
    for (auto t : tok.tokenize(norm)) trie->insert(t, id);
  }
  return std::make_shared<index::IndexView>(trie, table, arena, 1);
}
}  // namespace

TEST(ServingNodeE2E, SmallCorpusTopThreeForRe) {
  auto view = load(std::string(FIXTURE_DIR) + "/small_corpus.csv");
  query::QueryExecutor qe;

  std::string q = "re";
  auto r = qe.execute(*view, q, 3);
  ASSERT_EQ(r.size(), 3u);
  EXPECT_EQ(r[0].phrase, "region");
  EXPECT_EQ(r[1].phrase, "revenue");
  EXPECT_EQ(r[2].phrase, "revenues");
}
