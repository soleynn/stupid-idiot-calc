#include "calc/lexer.hpp"

#include <cmath>
#include <string>
#include <system_error>

#include <fast_float/fast_float.h>

#include "calc/limits.hpp"

namespace calc {

namespace {

bool is_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool is_digit(char c) { return c >= '0' && c <= '9'; }

bool is_alpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_ident_char(char c) { return is_alpha(c) || is_digit(c); }

} // namespace

Result<std::vector<Token>> tokenize(std::string_view input) {
  std::vector<Token> tokens;
  const std::size_t n = input.size();
  std::size_t i = 0;

  while (i < n) {
    // stop before the token vector can balloon. the only complexity cap used to
    // live in parse(), which runs after the whole vector is built - so a
    // multi-mb line ballooned into a multi-gb vector before the size check
    // rejected it. bail right here at the same cap, with the same error, the
    // moment the count passes it.
    if (tokens.size() > kMaxTokens) {
      return CalcError{ErrorKind::TooComplex, "expression is too long"};
    }

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
    case '%':
      single = TokenType::Percent;
      break;
    case '^':
      single = TokenType::Caret;
      break;
    case '=':
      single = TokenType::Equals;
      break;
    case '(':
      single = TokenType::LParen;
      break;
    case ')':
      single = TokenType::RParen;
      break;
    case ',':
      single = TokenType::Comma;
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
      // read the whole number with fast_float (locale independent, no throw).
      const char *first = input.data() + i;
      const char *last = input.data() + n;
      Number value = 0.0;
      const fast_float::from_chars_result fc =
          fast_float::from_chars(first, last, value);
      if (fc.ec == std::errc::result_out_of_range) {
        // fast_float flags both directions as out-of-range. an overflow comes
        // back as +/-inf and is a real error; an underflow (a literal too small
        // for a double, like 1e-400) comes back as 0, which is a perfectly good
        // answer - keep it instead of rejecting it. span the whole literal.
        if (std::isinf(value)) {
          const auto len = static_cast<std::size_t>(fc.ptr - first);
          return CalcError{ErrorKind::Overflow, "number is out of range",
                           SourceSpan{i, len}};
        }
      } else if (fc.ec != std::errc()) {
        return CalcError{ErrorKind::UnexpectedChar, "could not read a number",
                         SourceSpan{i, 1}};
      }
      tok.type = TokenType::Num;
      tok.value = value; // the parsed number, or 0 on an underflow
      tokens.push_back(tok);
      i += static_cast<std::size_t>(fc.ptr - first);
      continue;
    }

    if (is_alpha(c)) {
      // a name: a function or constant.
      std::size_t j = i;
      while (j < n && is_ident_char(input[j])) {
        ++j;
      }
      tok.type = TokenType::Ident;
      tok.text = std::string(input.substr(i, j - i));
      tokens.push_back(std::move(tok));
      i = j;
      continue;
    }

    if (c == '\0') {
      // a NUL would otherwise read as an empty-looking "unexpected character
      // ''" (the literal NUL closes the quoted char early). name it instead.
      return CalcError{ErrorKind::UnexpectedChar,
                       "unexpected NUL byte in input", SourceSpan{i, 1}};
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
