#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string_view>

#include "calc/environment.hpp"
#include "calc/evaluator.hpp"

using namespace calc;

namespace {

// evaluate a string and require it succeeded, returning the number. keeps the
// arithmetic cases to one readable line each.
Number value_of(std::string_view input) {
  Environment env;
  auto result = evaluate(input, env);
  REQUIRE(result.has_value());
  return result.value();
}

// the mirror for the failure cases: require it errored, hand back the kind.
ErrorKind error_kind_of(std::string_view input) {
  Environment env;
  auto result = evaluate(input, env);
  REQUIRE_FALSE(result.has_value());
  return result.error().kind;
}

} // namespace

TEST_CASE("evaluate returns a single number") {
  REQUIRE(value_of("42") == 42.0);
}

TEST_CASE("evaluate computes with the right precedence") {
  REQUIRE(value_of("2 + 3 * 4") == 14.0);
  REQUIRE(value_of("2 * 3 + 4") == 10.0);
  REQUIRE(value_of("(2 + 3) * 4") == 20.0);
}

TEST_CASE("evaluate is left-associative for - and /") {
  REQUIRE(value_of("8 - 3 - 2") == 3.0);
  REQUIRE(value_of("20 / 5 / 2") == 2.0);
}

TEST_CASE("evaluate handles unary minus") {
  REQUIRE(value_of("-5") == -5.0);
  REQUIRE(value_of("3 + -2") == 1.0);
  REQUIRE(value_of("-(2 + 3)") == -5.0);
}

TEST_CASE("evaluate handles the exponent operator") {
  REQUIRE(value_of("2 ^ 3") == 8.0);
  REQUIRE(value_of("2 ^ 3 ^ 2") == 512.0);
  REQUIRE(value_of("2 * 3 ^ 2") == 18.0);
  REQUIRE(value_of("-2 ^ 2") == -4.0);
  REQUIRE(value_of("(-2) ^ 2") == 4.0);
  REQUIRE(value_of("2 ^ -2") == 0.25);
  REQUIRE(value_of("9 ^ 0.5") == Catch::Approx(3.0));
}

TEST_CASE("an overflowing power is caught, not returned as inf") {
  REQUIRE(error_kind_of("10 ^ 400") == ErrorKind::Overflow);
}

TEST_CASE("evaluate handles the modulo operator") {
  REQUIRE(value_of("10 % 3") == 1.0);
  REQUIRE(value_of("5.5 % 2") == 1.5); // a real remainder, not integer-only
  REQUIRE(value_of("-7 % 3") == -1.0); // the result takes the dividend's sign
  REQUIRE(value_of("7 % -3") == 1.0);  // ...and ignores the divisor's
}

TEST_CASE("modulo by zero is an error, not NaN") {
  REQUIRE(error_kind_of("5 % 0") == ErrorKind::DivideByZero);
}

TEST_CASE("evaluate resolves the built-in constants") {
  REQUIRE(value_of("pi") == Catch::Approx(3.14159265358979));
  REQUIRE(value_of("e") == Catch::Approx(2.71828182845905));
}

TEST_CASE("evaluate computes the named functions") {
  REQUIRE(value_of("sqrt(9)") == 3.0);
  REQUIRE(value_of("abs(-5)") == 5.0);
  REQUIRE(value_of("floor(3.7)") == 3.0);
  REQUIRE(value_of("ceil(3.2)") == 4.0);
  REQUIRE(value_of("exp(0)") == 1.0);
  REQUIRE(value_of("ln(1)") == 0.0);
  REQUIRE(value_of("log(100)") == Catch::Approx(2.0));
}

TEST_CASE("trig functions work in degrees") {
  REQUIRE(value_of("sin(30)") == Catch::Approx(0.5));
  REQUIRE(value_of("cos(60)") == Catch::Approx(0.5));
  REQUIRE(value_of("tan(45)") == Catch::Approx(1.0));
}

TEST_CASE("function arguments are evaluated and their errors propagate") {
  REQUIRE(value_of("sqrt(abs(-9))") == 3.0); // nested calls
  REQUIRE(error_kind_of("sqrt(1 / 0)") == ErrorKind::DivideByZero);
}

TEST_CASE("domain errors come back as errors, not nan") {
  REQUIRE(error_kind_of("sqrt(-1)") == ErrorKind::DomainError);
  REQUIRE(error_kind_of("ln(0)") == ErrorKind::DomainError);
}

TEST_CASE("tan at its poles is a domain error, not a huge finite number") {
  REQUIRE(error_kind_of("tan(90)") == ErrorKind::DomainError);
  REQUIRE(error_kind_of("tan(270)") == ErrorKind::DomainError);
  REQUIRE(error_kind_of("tan(-90)") == ErrorKind::DomainError);
}

TEST_CASE("a negative base to a fractional power is a domain error, not nan") {
  REQUIRE(error_kind_of("(-2) ^ 0.5") == ErrorKind::DomainError);
  REQUIRE(error_kind_of("(-8) ^ (1 / 3)") == ErrorKind::DomainError);
  REQUIRE(value_of("(-2) ^ 3") ==
          -8.0); // a whole-number exponent is still fine
}

TEST_CASE("constants and functions resolve regardless of case") {
  REQUIRE(value_of("PI") == Catch::Approx(3.14159265358979));
  REQUIRE(value_of("SQRT(9)") == 3.0);
  REQUIRE(value_of("Abs(-4)") == 4.0);
}

TEST_CASE("an overflowing function result is caught, not returned as inf") {
  REQUIRE(error_kind_of("exp(1000)") == ErrorKind::Overflow);
}

TEST_CASE("unknown names and functions are errors") {
  REQUIRE(error_kind_of("xyz") == ErrorKind::UnknownName);
  REQUIRE(error_kind_of("foo(2)") == ErrorKind::UnknownName);
}

TEST_CASE("functions called with the wrong number of arguments error") {
  REQUIRE(error_kind_of("sqrt()") == ErrorKind::WrongArgCount);
  REQUIRE(error_kind_of("sqrt(1, 2)") == ErrorKind::WrongArgCount);
}

TEST_CASE("evaluate handles fractional results") {
  REQUIRE(value_of("3 / 4") == Catch::Approx(0.75));
  REQUIRE(value_of("10 / 3") == Catch::Approx(3.3333).epsilon(0.001));
}

TEST_CASE("evaluate reports divide by zero instead of infinity") {
  REQUIRE(error_kind_of("1 / 0") == ErrorKind::DivideByZero);
  // a divisor that computes to zero must be caught too, not just a literal 0.
  REQUIRE(error_kind_of("5 / (3 - 3)") == ErrorKind::DivideByZero);
}

TEST_CASE("an error in a sub-expression bubbles up to the top") {
  // a 1/0 buried in a left operand, a right operand, and a unary operand: the
  // error has to propagate back out through each kind of parent node.
  REQUIRE(error_kind_of("1 / 0 + 1") == ErrorKind::DivideByZero);
  REQUIRE(error_kind_of("1 + 1 / 0") == ErrorKind::DivideByZero);
  REQUIRE(error_kind_of("-(1 / 0)") == ErrorKind::DivideByZero);
}

TEST_CASE("evaluate reports overflow instead of infinity") {
  REQUIRE(error_kind_of("1e308 * 1e308") == ErrorKind::Overflow);
}

TEST_CASE("evaluate reports empty input") {
  REQUIRE(error_kind_of("   ") == ErrorKind::EmptyInput);
}

TEST_CASE("evaluate does not crash on garbage") {
  REQUIRE(error_kind_of("@#$") == ErrorKind::UnexpectedChar);
}

TEST_CASE("evaluate passes a parse error straight through") {
  REQUIRE(error_kind_of("2 + * 3") == ErrorKind::UnexpectedToken);
}
