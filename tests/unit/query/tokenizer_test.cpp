#include <gtest/gtest.h>
#include <vector>
#include "autocomplete/query/Tokenizer.h"

using autocomplete::query::Tokenizer;

namespace {
std::vector<std::string> as_strings(const std::vector<std::string_view>& v) {
  return {v.begin(), v.end()};
}
}  // namespace

TEST(TokenizerTest, SplitsOnWhitespace) {
  Tokenizer tok;
  std::string in = "total revenue q4";
  EXPECT_EQ(as_strings(tok.tokenize(in)),
            (std::vector<std::string>{"total", "revenue", "q4"}));
}

TEST(TokenizerTest, SplitsOnPunctuationAndLowercases) {
  Tokenizer tok;
  std::string in = "Sales/Region, 2025!";
  EXPECT_EQ(as_strings(tok.tokenize(in)),
            (std::vector<std::string>{"sales", "region", "2025"}));
}

TEST(TokenizerTest, EmptyInputProducesNoTokens) {
  Tokenizer tok;
  std::string in;
  EXPECT_TRUE(tok.tokenize(in).empty());
}

TEST(TokenizerTest, DropsEmptyRunsFromRepeatedSeparators) {
  Tokenizer tok;
  std::string in = "  a,, ,b ";
  EXPECT_EQ(as_strings(tok.tokenize(in)),
            (std::vector<std::string>{"a", "b"}));
}
