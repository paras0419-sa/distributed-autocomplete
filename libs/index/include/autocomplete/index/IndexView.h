#pragma once
#include <cstdint>
#include <memory>
#include "autocomplete/index/EntryTable.h"
#include "autocomplete/index/FstIndex.h"
#include "autocomplete/memory/StringArena.h"

namespace autocomplete::index {

class IndexView {
 public:
  IndexView(std::shared_ptr<FstIndex>                  idx,
            std::shared_ptr<EntryTable>                tbl,
            std::shared_ptr<memory::StringArena>       arena,
            std::uint64_t                              version)
      : idx_{std::move(idx)},
        tbl_{std::move(tbl)},
        arena_{std::move(arena)},
        version_{version} {}

  const FstIndex&             index()   const noexcept { return *idx_; }
  const EntryTable&           entries() const noexcept { return *tbl_; }
  const memory::StringArena&  arena()   const noexcept { return *arena_; }
  std::uint64_t               version() const noexcept { return version_; }

 private:
  std::shared_ptr<FstIndex>            idx_;
  std::shared_ptr<EntryTable>          tbl_;
  std::shared_ptr<memory::StringArena> arena_;
  std::uint64_t                        version_;
};

}  // namespace autocomplete::index
