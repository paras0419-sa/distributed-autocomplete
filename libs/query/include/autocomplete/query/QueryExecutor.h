#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "autocomplete/index/IndexView.h"

namespace autocomplete::query {

struct Suggestion {
  std::string_view  phrase;  // view into IndexView::arena; valid while view lives
  float             weight;
  index::SourceFlag source;
};

class QueryExecutor {
 public:
  // Tokenizes `prefix` (caller owns, modified to lowercase) and returns
  // top-K suggestions by weight descending. Phase 1 semantics: matches on
  // the last token's prefix; leading tokens are ignored.
  std::vector<Suggestion> execute(const index::IndexView& view,
                                  std::string&           prefix,
                                  std::size_t            k) const;
};

}  // namespace autocomplete::query
