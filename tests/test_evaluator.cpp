#include <catch2/catch_test_macros.hpp>

#include "calc/environment.hpp"
#include "calc/evaluator.hpp"

using namespace calc;

TEST_CASE("evaluate returns a single number") {
  Environment env;
  auto r = evaluate("42", env);
  REQUIRE(r.has_value());
  REQUIRE(r.value() == 42.0);
}

TEST_CASE("evaluate reports empty input") {
  Environment env;
  auto r = evaluate("   ", env);
  REQUIRE_FALSE(r.has_value());
  REQUIRE(r.error().kind == ErrorKind::EmptyInput);
}

TEST_CASE("evaluate does not crash on garbage") {
  Environment env;
  auto r = evaluate("@#$", env);
  REQUIRE_FALSE(r.has_value());
  REQUIRE(r.error().kind == ErrorKind::UnexpectedChar);
}

TEST_CASE("evaluate does not handle full expressions yet") {
  Environment env;
  auto r = evaluate("1 + 2", env);
  REQUIRE_FALSE(r.has_value());
  REQUIRE(r.error().kind == ErrorKind::NotImplemented);
}
