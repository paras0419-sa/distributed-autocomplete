#include "autocomplete/query/Tokenizer.h"
#include <cctype>

namespace autocomplete::query {

bool Tokenizer::is_separator(char c) noexcept {
  return std::isspace(static_cast<unsigned char>(c)) ||
         std::ispunct(static_cast<unsigned char>(c));
}

std::vector<std::string_view> Tokenizer::tokenize(std::string& buf) const {
  for (char& c : buf) c = static_cast<char>(
      std::tolower(static_cast<unsigned char>(c)));

  std::vector<std::string_view> out;
  std::size_t i = 0, n = buf.size();
  while (i < n) {
    while (i < n && is_separator(buf[i])) ++i;
    std::size_t start = i;
    while (i < n && !is_separator(buf[i])) ++i;
    if (i > start) out.emplace_back(buf.data() + start, i - start);
  }
  return out;
}

}  // namespace autocomplete::query
