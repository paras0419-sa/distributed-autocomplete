#pragma once
#include <utility>
#include <variant>
#include "autocomplete/common/Error.h"

namespace autocomplete::common {

template <typename T>
class Result {
 public:
  static Result ok(T v)         { return Result{std::move(v)}; }
  static Result err(Error e)    { return Result{std::move(e)}; }

  bool has_value() const noexcept { return data_.index() == 0; }
  explicit operator bool() const noexcept { return has_value(); }

  T&       operator*()       { return std::get<0>(data_); }
  const T& operator*() const { return std::get<0>(data_); }

  const Error& error() const { return std::get<1>(data_); }

 private:
  explicit Result(T v)     : data_{std::in_place_index<0>, std::move(v)} {}
  explicit Result(Error e) : data_{std::in_place_index<1>, std::move(e)} {}
  std::variant<T, Error> data_;
};

}  // namespace autocomplete::common
