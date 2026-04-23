#pragma once
#include <string>
#include <string_view>

namespace autocomplete::common {

enum class ErrorCode {
  kInvalidInput,
  kNotFound,
  kInternal,
};

constexpr const char* to_string(ErrorCode c) {
  switch (c) {
    case ErrorCode::kInvalidInput: return "InvalidInput";
    case ErrorCode::kNotFound:     return "NotFound";
    case ErrorCode::kInternal:     return "Internal";
  }
  return "Unknown";
}

class Error {
 public:
  Error(ErrorCode c, std::string msg) : code_{c}, message_{std::move(msg)} {}
  ErrorCode code() const noexcept { return code_; }
  const std::string& message() const noexcept { return message_; }

 private:
  ErrorCode   code_;
  std::string message_;
};

}  // namespace autocomplete::common
