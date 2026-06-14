#pragma once

#include <cstddef>

#include "calc/number.hpp"

namespace calc {

enum class TokenType {
  Num,
  Plus,
  Minus,
  Star,
  Slash,
  LParen,
  RParen,
  End,
};

// one lexical atom. plain data, no behavior.
struct Token {
  TokenType type = TokenType::End;
  Number value = 0.0;     // only meaningful when type == Num
  std::size_t offset = 0; // byte position in the source, for error spans
};

} // namespace calc
