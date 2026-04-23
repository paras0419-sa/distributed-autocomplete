#include "autocomplete/query/QueryExecutor.h"
#include "autocomplete/query/FixedTopK.h"
#include "autocomplete/query/Tokenizer.h"

namespace autocomplete::query {

namespace {
struct Scored {
  index::EntryId id;
  float          weight;
};
struct ByWeight {
  bool operator()(const Scored& a, const Scored& b) const {
    return a.weight < b.weight;
  }
};
}  // namespace

std::vector<Suggestion> QueryExecutor::execute(const index::IndexView& view,
                                               std::string&            prefix,
                                               std::size_t             k) const {
  Tokenizer tok;
  auto tokens = tok.tokenize(prefix);
  if (tokens.empty() || k == 0) return {};

  const std::string_view last = tokens.back();
  FixedTopK<Scored, ByWeight> heap{k};

  view.index().walk_prefix(last, [&](index::EntryId id) {
    const auto& e = view.entries().at(id);
    heap.offer({id, e.weight});
    return true;  // no early stop in Phase 1; revisited in Phase 2
  });

  auto sorted = heap.take_sorted();
  std::vector<Suggestion> out;
  out.reserve(sorted.size());
  for (const auto& s : sorted) {
    const auto& e = view.entries().at(s.id);
    out.push_back(Suggestion{
        view.arena().view({e.phrase_offset, e.phrase_length}),
        e.weight, e.source_flag});
  }
  return out;
}

}  // namespace autocomplete::query
