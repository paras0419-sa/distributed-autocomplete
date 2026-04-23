#include <gtest/gtest.h>
#include "autocomplete/common/Error.h"

using autocomplete::common::Error;
using autocomplete::common::ErrorCode;

TEST(ErrorTest, ConstructCarriesCodeAndMessage) {
  Error e{ErrorCode::kInvalidInput, "bad prefix"};
  EXPECT_EQ(e.code(), ErrorCode::kInvalidInput);
  EXPECT_EQ(e.message(), "bad prefix");
}

TEST(ErrorTest, CodeToStringKnownCodes) {
  EXPECT_STREQ(to_string(ErrorCode::kInvalidInput), "InvalidInput");
  EXPECT_STREQ(to_string(ErrorCode::kNotFound), "NotFound");
  EXPECT_STREQ(to_string(ErrorCode::kInternal), "Internal");
}
