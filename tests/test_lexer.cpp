#include <catch2/catch_test_macros.hpp>

#include "calc/lexer.hpp"

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

TEST_CASE("tokenize reports out-of-range numbers") {
  auto r = tokenize("1e400");
  REQUIRE_FALSE(r.has_value());
  REQUIRE(r.error().kind == ErrorKind::Overflow);
  REQUIRE(r.error().span.length == 5u); // the whole literal, not one char
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
