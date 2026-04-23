#pragma once
#include <cstdint>
#include <functional>
#include <string_view>
#include "autocomplete/index/Entry.h"

namespace autocomplete::index {

// Visitor receives entry IDs associated with tokens whose text starts with the
// queried prefix. Returning false stops the walk early.
using PrefixVisitor = std::function<bool(EntryId)>;

class FstIndex {
 public:
  virtual ~FstIndex() = default;

  // Register a token → entry_id edge. One token may map to multiple entry_ids.
  virtual void insert(std::string_view token, EntryId id) = 0;

  // Invoke `visit` for every entry_id whose token has `prefix` as a prefix.
  virtual void walk_prefix(std::string_view prefix, const PrefixVisitor& visit) const = 0;

  virtual std::size_t token_count() const noexcept = 0;
};

}  // namespace autocomplete::index
