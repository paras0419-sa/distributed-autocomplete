#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "autocomplete/index/EntryTable.h"
#include "autocomplete/index/IndexView.h"
#include "autocomplete/index/SimpleTrie.h"
#include "autocomplete/memory/StringArena.h"
#include "autocomplete/query/QueryExecutor.h"
#include "autocomplete/query/Tokenizer.h"

using namespace autocomplete;

namespace {

std::shared_ptr<index::IndexView> load_corpus(const std::string& path) {
  auto arena = std::make_shared<memory::StringArena>();
  auto table = std::make_shared<index::EntryTable>();
  auto trie  = std::make_shared<index::SimpleTrie>();
  query::Tokenizer tok;

  std::ifstream f(path);
  if (!f) {
    std::fprintf(stderr, "cannot open corpus: %s\n", path.c_str());
    std::exit(2);
  }

  std::string line;
  std::size_t rows = 0;
  while (std::getline(f, line)) {
    if (line.empty()) continue;
    auto comma = line.find_last_of(',');
    if (comma == std::string::npos) continue;
    std::string phrase = line.substr(0, comma);
    float weight = std::stof(line.substr(comma + 1));

    auto h  = arena->append(phrase);
    auto id = table->add(index::Entry{
        h.offset, weight, /*last_seen=*/0, h.length,
        index::SourceFlag::kColumn, 0});

    // Tokenize a lowercased copy so the trie sees normalized tokens.
    std::string norm = phrase;
    auto tokens = tok.tokenize(norm);
    for (auto t : tokens) trie->insert(t, id);
    ++rows;
  }
  std::fprintf(stderr, "loaded %zu phrases, %zu tokens, arena=%zu bytes\n",
               rows, trie->token_count(), arena->bytes());
  return std::make_shared<index::IndexView>(trie, table, arena, 1);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: serving_node <corpus.csv>\n");
    return 1;
  }

  auto view = load_corpus(argv[1]);
  query::QueryExecutor qe;

  std::string line;
  std::fprintf(stderr, "ready. type a prefix, ^D to quit.\n> ");
  while (std::getline(std::cin, line)) {
    auto results = qe.execute(*view, line, /*k=*/10);
    for (const auto& s : results) {
      std::cout << s.phrase << "\t" << s.weight << "\n";
    }
    std::cout.flush();
    std::fprintf(stderr, "> ");
  }
  return 0;
}
