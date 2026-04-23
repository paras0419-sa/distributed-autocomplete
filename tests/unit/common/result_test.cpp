#include <gtest/gtest.h>
#include <string>
#include "autocomplete/common/Result.h"

using autocomplete::common::Result;
using autocomplete::common::Error;
using autocomplete::common::ErrorCode;

TEST(ResultTest, OkCarriesValue) {
  Result<int> r = Result<int>::ok(42);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, 42);
}

TEST(ResultTest, ErrCarriesError) {
  Result<int> r = Result<int>::err(Error{ErrorCode::kNotFound, "missing"});
  ASSERT_FALSE(r.has_value());
  EXPECT_EQ(r.error().code(), ErrorCode::kNotFound);
}

TEST(ResultTest, MoveOnlyValueTypeSupported) {
  Result<std::string> r = Result<std::string>::ok("hello");
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, "hello");
  std::string moved = std::move(*r);
  EXPECT_EQ(moved, "hello");
}
