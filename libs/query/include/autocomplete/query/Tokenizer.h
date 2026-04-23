#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace autocomplete::query {

class Tokenizer {
 public:
  // Lowercases `buf` in place, then returns string_views pointing into it.
  // Caller must keep `buf` alive for the lifetime of the returned views.
  std::vector<std::string_view> tokenize(std::string& buf) const;

 private:
  static bool is_separator(char c) noexcept;
};

}  // namespace autocomplete::query
