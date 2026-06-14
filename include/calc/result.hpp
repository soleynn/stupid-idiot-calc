#pragma once

#include <cassert>
#include <utility>
#include <variant>

#include "calc/error.hpp"

namespace calc {

// holds either a T or a CalcError. a tiny stand-in for std::expected (c++23),
// good enough until the standard gets bumped. the implicit constructors let
// the engine just `return some_number;` or `return some_error;`.
template <typename T> class Result {
public:
  Result(T val) : data_(std::move(val)) {}
  Result(CalcError err) : data_(std::move(err)) {}

  bool has_value() const { return std::holds_alternative<T>(data_); }
  explicit operator bool() const { return has_value(); }

  const T &value() const {
    assert(has_value() && "Result::value() called on an error");
    return std::get<T>(data_);
  }

  const CalcError &error() const {
    assert(!has_value() && "Result::error() called on a value");
    return std::get<CalcError>(data_);
  }

private:
  std::variant<T, CalcError> data_;
};

} // namespace calc
