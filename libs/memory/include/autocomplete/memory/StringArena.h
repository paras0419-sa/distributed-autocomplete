#pragma once
#include <cstdint>
#include <string_view>
#include <vector>

namespace autocomplete::memory {

struct StringHandle {
  std::uint32_t offset;
  std::uint16_t length;
};

class StringArena {
 public:
  StringHandle append(std::string_view s) {
    const auto off = static_cast<std::uint32_t>(buf_.size());
    buf_.insert(buf_.end(), s.begin(), s.end());
    return StringHandle{off, static_cast<std::uint16_t>(s.size())};
  }

  std::string_view view(StringHandle h) const noexcept {
    return std::string_view{buf_.data() + h.offset, h.length};
  }

  std::size_t bytes() const noexcept { return buf_.size(); }

 private:
  std::vector<char> buf_;
};

}  // namespace autocomplete::memory
