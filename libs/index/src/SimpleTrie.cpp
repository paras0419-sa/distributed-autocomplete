#include "autocomplete/index/SimpleTrie.h"

namespace autocomplete::index {

void SimpleTrie::insert(std::string_view token, EntryId id) {
  if (token.empty()) return;
  Node* cur = &root_;
  for (char c : token) {
    auto& slot = cur->kids[c];
    if (!slot) slot = std::make_unique<Node>();
    cur = slot.get();
  }
  cur->entries.push_back(id);
  ++token_count_;
}

bool SimpleTrie::visit_subtree(const Node* n, const PrefixVisitor& visit) {
  for (EntryId id : n->entries) {
    if (!visit(id)) return false;
  }
  for (const auto& [c, child] : n->kids) {
    if (!visit_subtree(child.get(), visit)) return false;
  }
  return true;
}

void SimpleTrie::walk_prefix(std::string_view prefix,
                             const PrefixVisitor& visit) const {
  if (prefix.empty()) return;
  const Node* cur = &root_;
  for (char c : prefix) {
    auto it = cur->kids.find(c);
    if (it == cur->kids.end()) return;
    cur = it->second.get();
  }
  visit_subtree(cur, visit);
}

}  // namespace autocomplete::index
