#include <catch2/catch_test_macros.hpp>

#include "calc/diagnostic.hpp"
#include "calc/environment.hpp"
#include "calc/error.hpp"
#include "calc/evaluator.hpp"

using namespace calc;

TEST_CASE("diagnostic underlines the offending token with a column") {
  Environment env;
  auto result = evaluate("2 + * 3", env);
  REQUIRE_FALSE(result.has_value());
  REQUIRE(render_diagnostic("2 + * 3", result.error()) ==
          "error: expected a number, name, or '(' (column 5)\n"
          "2 + * 3\n"
          "    ^");
}

TEST_CASE("diagnostic underlines a whole multi-character span") {
  CalcError err{ErrorKind::Overflow, "number is out of range",
                SourceSpan{0, 5}};
  REQUIRE(render_diagnostic("1e400 + 2", err) ==
          "error: number is out of range (column 1)\n"
          "1e400 + 2\n"
          "^~~~~");
}

TEST_CASE("diagnostic points just past the end for an unfinished expression") {
  CalcError err{ErrorKind::UnexpectedToken, "expected a number, name, or '('",
                SourceSpan{3, 0}};
  REQUIRE(render_diagnostic("1 +", err) ==
          "error: expected a number, name, or '(' (column 4)\n"
          "1 +\n"
          "   ^");
}

TEST_CASE("diagnostic copies a leading tab so the caret stays lined up") {
  Environment env;
  auto result = evaluate("\t* 3", env);
  REQUIRE_FALSE(result.has_value());
  REQUIRE(render_diagnostic("\t* 3", result.error()) ==
          "error: expected a number, name, or '(' (column 2)\n"
          "\t* 3\n"
          "\t^");
}

TEST_CASE("diagnostic skips the caret when the error has no location") {
  CalcError err{ErrorKind::DivideByZero, "cant divide by zero"};
  REQUIRE(render_diagnostic("1 / 0", err) == "error: cant divide by zero");
}

TEST_CASE("diagnostic underlines a whole multi-character error token") {
  Environment env;
  auto result = evaluate("1 23456789", env);
  REQUIRE_FALSE(result.has_value());
  REQUIRE(render_diagnostic("1 23456789", result.error()) ==
          "error: unexpected input after the expression (column 3)\n"
          "1 23456789\n"
          "  ^~~~~~~~");
}

TEST_CASE("diagnostic counts characters, not bytes, under multibyte utf-8") {
  // "é+$": é is 2 bytes (C3 A9) but one glyph, so the caret must sit under '$'
  // at character column 3 with two columns of padding, not three.
  const CalcError err{ErrorKind::UnexpectedChar, "unexpected character '$'",
                      SourceSpan{3, 1}};
  REQUIRE(render_diagnostic("\xC3\xA9+$", err) ==
          "error: unexpected character '$' (column 3)\n"
          "\xC3\xA9+$\n"
          "  ^");
}
