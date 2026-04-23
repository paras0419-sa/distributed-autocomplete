#pragma once
#include <cstdint>

namespace autocomplete::index {

enum class SourceFlag : std::uint8_t {
  kColumn     = 0,
  kPastQuery  = 1,
  kDataValue  = 2,
};

// Field order chosen to keep sizeof(Entry) == 16 on common ABIs (no holes
// from aligning `float` after a 16-bit field).
struct Entry {
  std::uint32_t phrase_offset;   // 0..3   into StringArena
  float         weight;          // 4..7
  std::uint32_t last_seen_epoch; // 8..11
  std::uint16_t phrase_length;   // 12..13
  SourceFlag    source_flag;     // 14
  std::uint8_t  _pad;            // 15
};

static_assert(sizeof(Entry) == 16, "Entry layout drift");

using EntryId = std::uint32_t;

}  // namespace autocomplete::index
