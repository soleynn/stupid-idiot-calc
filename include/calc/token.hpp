#pragma once

#include <cstddef>
#include <string>

#include "calc/number.hpp"

namespace calc {

enum class TokenType {
  Num,
  Ident,
  Plus,
  Minus,
  Star,
  Slash,
  Percent,
  Caret,
  LParen,
  RParen,
  Comma,
  End,
};

// one lexical atom. plain data, no behavior.
struct Token {
  TokenType type = TokenType::End;
  Number value = 0.0;     // only meaningful when type == Num
  std::string text;       // the name, only meaningful when type == Ident
  std::size_t offset = 0; // byte position in the source, for error spans
};

} // namespace calc
