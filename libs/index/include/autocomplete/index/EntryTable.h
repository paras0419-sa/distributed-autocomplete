#pragma once
#include <vector>
#include "autocomplete/index/Entry.h"

namespace autocomplete::index {

class EntryTable {
 public:
  EntryId add(Entry e) {
    entries_.push_back(e);
    return static_cast<EntryId>(entries_.size() - 1);
  }

  const Entry& at(EntryId id) const { return entries_[id]; }
  std::size_t  size()        const noexcept { return entries_.size(); }

 private:
  std::vector<Entry> entries_;
};

}  // namespace autocomplete::index
