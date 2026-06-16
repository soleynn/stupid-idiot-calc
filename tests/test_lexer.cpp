#include <catch2/catch_test_macros.hpp>

#include <string>

#include "calc/lexer.hpp"
#include "calc/limits.hpp"

using namespace calc;

TEST_CASE("tokenize reads numbers and operators") {
  auto r = tokenize("1 + 2");
  REQUIRE(r.has_value());
  const auto &toks = r.value();
  REQUIRE(toks.size() == 4u); // 1, +, 2, End
  REQUIRE(toks[0].type == TokenType::Num);
  REQUIRE(toks[0].value == 1.0);
  REQUIRE(toks[1].type == TokenType::Plus);
  REQUIRE(toks[2].type == TokenType::Num);
  REQUIRE(toks[2].value == 2.0);
  REQUIRE(toks[3].type == TokenType::End);
}

TEST_CASE("tokenize handles parentheses and decimals") {
  auto r = tokenize("(3.5)");
  REQUIRE(r.has_value());
  const auto &toks = r.value();
  REQUIRE(toks.size() == 4u); // (, 3.5, ), End
  REQUIRE(toks[0].type == TokenType::LParen);
  REQUIRE(toks[1].type == TokenType::Num);
  REQUIRE(toks[1].value == 3.5);
  REQUIRE(toks[2].type == TokenType::RParen);
}

TEST_CASE("tokenize skips surrounding whitespace") {
  auto r = tokenize("   7   ");
  REQUIRE(r.has_value());
  REQUIRE(r.value().size() == 2u); // 7, End
}

TEST_CASE("tokenize rejects unknown characters") {
  auto r = tokenize("1 $ 2");
  REQUIRE_FALSE(r.has_value());
  REQUIRE(r.error().kind == ErrorKind::UnexpectedChar);
}

TEST_CASE("tokenize rejects an embedded NUL byte") {
  // a NUL interior to the input (the kind a piped repl line can smuggle in)
  // must be an error, not silently dropped so the bytes around it merge.
  std::string input = "2+2";
  input.push_back('\0');
  input += "2+3";
  auto r = tokenize(input);
  REQUIRE_FALSE(r.has_value());
  REQUIRE(r.error().kind == ErrorKind::UnexpectedChar);
  REQUIRE(r.error().message == "unexpected NUL byte in input");
  REQUIRE(r.error().span.offset == 3u); // points right at the NUL
}

TEST_CASE("tokenize reports out-of-range numbers") {
  auto r = tokenize("1e400");
  REQUIRE_FALSE(r.has_value());
  REQUIRE(r.error().kind == ErrorKind::Overflow);
  REQUIRE(r.error().span.length == 5u); // the whole literal, not one char
}

TEST_CASE("tokenize underflows a too-small number to zero, not an error") {
  // fast_float flags both 1e400 and 1e-400 as out-of-range, but the tiny one is
  // an underflow to 0 (a fine answer), not the overflow the huge one is.
  auto r = tokenize("1e-400");
  REQUIRE(r.has_value());
  REQUIRE(r.value().size() == 2u); // the number, then End
  REQUIRE(r.value()[0].type == TokenType::Num);
  REQUIRE(r.value()[0].value == 0.0);
}

TEST_CASE("tokenize reads the equals sign") {
  auto r = tokenize("x = 1");
  REQUIRE(r.has_value());
  const auto &toks = r.value();
  REQUIRE(toks.size() == 4u); // x = 1 End
  REQUIRE(toks[0].type == TokenType::Ident);
  REQUIRE(toks[1].type == TokenType::Equals);
  REQUIRE(toks[2].type == TokenType::Num);
}

TEST_CASE("tokenize reads names and commas") {
  auto r = tokenize("sqrt(pi, 2)");
  REQUIRE(r.has_value());
  const auto &toks = r.value();
  REQUIRE(toks.size() == 7u); // sqrt ( pi , 2 ) End
  REQUIRE(toks[0].type == TokenType::Ident);
  REQUIRE(toks[0].text == "sqrt"); // the whole name, kept separate from the '('
  REQUIRE(toks[1].type == TokenType::LParen);
  REQUIRE(toks[2].type == TokenType::Ident);
  REQUIRE(toks[2].text == "pi");
  REQUIRE(toks[3].type == TokenType::Comma);
  REQUIRE(toks[4].type == TokenType::Num);
  REQUIRE(toks[4].value == 2.0);
  REQUIRE(toks[5].type == TokenType::RParen);
}

TEST_CASE("tokenize stops oversized input at the token cap") {
  // a flat run of operators is one token each. right at the cap it still
  // tokenizes (kMaxTokens-1 ops plus the End token = kMaxTokens); a
  // five-million char line is rejected outright instead of building the whole
  // multi-million element vector first, which is the cheap length gate that
  // used to only happen in parse() after the fact.
  const auto at_cap = tokenize(std::string(kMaxTokens - 1, '+'));
  REQUIRE(at_cap.has_value());
  REQUIRE(at_cap.value().size() == kMaxTokens);

  const auto over = tokenize(std::string(5'000'000, '+'));
  REQUIRE_FALSE(over.has_value());
  REQUIRE(over.error().kind == ErrorKind::TooComplex);
  REQUIRE(over.error().message == "expression is too long");
}
