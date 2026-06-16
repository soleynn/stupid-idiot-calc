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

// the byte length of the utf-8 codepoint starting at input[i], or 0 if it isnt
// a well-formed one (a stray continuation byte, or a lead byte missing its
// continuation bytes). lets an "unexpected character" error span and quote a
// whole non-ascii glyph instead of one byte of it.
std::size_t utf8_len(std::string_view input, std::size_t i) {
  const auto lead = static_cast<unsigned char>(input[i]);
  std::size_t len = 0;
  if (lead < 0x80) {
    len = 1;
  } else if (lead >= 0xC0 && lead < 0xE0) {
    len = 2;
  } else if (lead >= 0xE0 && lead < 0xF0) {
    len = 3;
  } else if (lead >= 0xF0 && lead < 0xF8) {
    len = 4;
  } else {
    return 0; // a continuation byte or invalid lead, not a codepoint start
  }
  for (std::size_t k = 1; k < len; ++k) {
    if (i + k >= input.size() ||
        (static_cast<unsigned char>(input[i + k]) & 0xC0) != 0x80) {
      return 0; // the continuation bytes arent there
    }
  }
  return len;
}

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
      tok.length = 1;
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
      tok.length = static_cast<std::size_t>(fc.ptr - first);
      tokens.push_back(tok);
      i += tok.length;
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
      tok.length = j - i;
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
    if (static_cast<unsigned char>(c) >= 0x80) {
      // a non-ascii byte: a utf-8 glyph the calc doesnt understand. quote and
      // span the whole codepoint so the message stays valid utf-8 and the caret
      // covers the character, not one byte of it.
      const std::size_t cp = utf8_len(input, i);
      if (cp >= 2) {
        return CalcError{ErrorKind::UnexpectedChar,
                         "unexpected character '" +
                             std::string(input.substr(i, cp)) + "'",
                         SourceSpan{i, cp}};
      }
      // a stray/invalid byte: dont splice it into the message (it'd be bad
      // utf-8); name it generically.
      return CalcError{ErrorKind::UnexpectedChar, "unexpected non-ASCII byte",
                       SourceSpan{i, 1}};
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
