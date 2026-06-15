#pragma once

#include <cstddef>
#include <string>
#include <utility>

namespace calc {

// where in the input something went wrong. byte offset + length, so a ui can
// point a caret at it later. unused by the cli for now.
struct SourceSpan {
  std::size_t offset = 0;
  std::size_t length = 0;
};

enum class ErrorKind {
  EmptyInput,
  UnexpectedChar,
  UnexpectedToken,
  UnbalancedParen,
  TooComplex, // nested too deep or too long to parse safely
  DivideByZero,
  Overflow,
  DomainError,   // a math domain error, like sqrt of a negative
  UnknownName,   // an unknown function or constant name
  WrongArgCount, // a function called with the wrong number of arguments
  NotImplemented,
};

// an expected, user-facing failure. carried around as a value, never thrown
// across the engine boundary.
struct CalcError {
  ErrorKind kind = ErrorKind::NotImplemented;
  std::string message;
  SourceSpan span;

  CalcError() = default;
  CalcError(ErrorKind k, std::string msg, SourceSpan s = {})
      : kind(k), message(std::move(msg)), span(s) {}
};

} // namespace calc
