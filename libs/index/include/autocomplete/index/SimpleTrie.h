#pragma once
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "autocomplete/index/FstIndex.h"

namespace autocomplete::index {

class SimpleTrie final : public FstIndex {
 public:
  void insert(std::string_view token, EntryId id) override;
  void walk_prefix(std::string_view prefix, const PrefixVisitor& visit) const override;
  std::size_t token_count() const noexcept override { return token_count_; }

 private:
  struct Node {
    std::unordered_map<char, std::unique_ptr<Node>> kids;
    std::vector<EntryId> entries;
  };

  // Walk subtree rooted at `n`, visiting every entry. Returns false if
  // visitor signalled stop.
  static bool visit_subtree(const Node* n, const PrefixVisitor& visit);

  Node root_;
  std::size_t token_count_ = 0;
};

}  // namespace autocomplete::index
