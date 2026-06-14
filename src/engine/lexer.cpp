#include "calc/lexer.hpp"

#include <charconv>
#include <string>
#include <system_error>

namespace calc {

namespace {

bool is_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool is_digit(char c) { return c >= '0' && c <= '9'; }

} // namespace

Result<std::vector<Token>> tokenize(std::string_view input) {
  std::vector<Token> tokens;
  const std::size_t n = input.size();
  std::size_t i = 0;

  while (i < n) {
    const char c = input[i];

    if (is_space(c)) {
      ++i;
      continue;
    }

    Token tok;
    tok.offset = i;

    TokenType single = TokenType::End;
    bool matched = true;
    switch (c) {
    case '+':
      single = TokenType::Plus;
      break;
    case '-':
      single = TokenType::Minus;
      break;
    case '*':
      single = TokenType::Star;
      break;
    case '/':
      single = TokenType::Slash;
      break;
    case '(':
      single = TokenType::LParen;
      break;
    case ')':
      single = TokenType::RParen;
      break;
    default:
      matched = false;
      break;
    }

    if (matched) {
      tok.type = single;
      tokens.push_back(tok);
      ++i;
      continue;
    }

    if (is_digit(c) || c == '.') {
      // read the whole number with from_chars (locale independent, no throw).
      const char *first = input.data() + i;
      const char *last = input.data() + n;
      Number value = 0.0;
      const std::from_chars_result fc = std::from_chars(first, last, value);
      if (fc.ec == std::errc::result_out_of_range) {
        // a valid number, just too big/small for a double. point the span at
        // the whole literal, not a single char.
        const std::size_t len = static_cast<std::size_t>(fc.ptr - first);
        return CalcError{ErrorKind::Overflow, "number is out of range",
                         SourceSpan{i, len}};
      }
      if (fc.ec != std::errc()) {
        return CalcError{ErrorKind::UnexpectedChar, "could not read a number",
                         SourceSpan{i, 1}};
      }
      tok.type = TokenType::Num;
      tok.value = value;
      tokens.push_back(tok);
      i += static_cast<std::size_t>(fc.ptr - first);
      continue;
    }

    return CalcError{ErrorKind::UnexpectedChar,
                     std::string("unexpected character '") + c + "'",
                     SourceSpan{i, 1}};
  }

  Token end;
  end.type = TokenType::End;
  end.offset = n;
  tokens.push_back(end);
  return tokens;
}

} // namespace calc
