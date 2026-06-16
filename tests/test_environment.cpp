#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string_view>

#include "calc/environment.hpp"
#include "calc/evaluator.hpp"

using namespace calc;

namespace {

// these features are stateful across lines, so the tests drive one env through
// several evaluate() calls. these helpers keep that to one line each.
Number value_of(Environment &env, std::string_view input) {
  auto result = evaluate(input, env);
  REQUIRE(result.has_value());
  return result.value();
}

ErrorKind error_kind_of(Environment &env, std::string_view input) {
  auto result = evaluate(input, env);
  REQUIRE_FALSE(result.has_value());
  return result.error().kind;
}

} // namespace

TEST_CASE("ans starts at zero before anything is computed") {
  Environment env;
  REQUIRE(value_of(env, "ans") == 0.0);
}

TEST_CASE("ans carries the last result into the next line") {
  Environment env;
  REQUIRE(value_of(env, "2 + 3") == 5.0);
  REQUIRE(value_of(env, "ans") == 5.0);
  REQUIRE(value_of(env, "ans * 2") == 10.0);
  REQUIRE(value_of(env, "ans") == 10.0); // the ans*2 result rolled forward
}

TEST_CASE("a failed line leaves ans untouched") {
  Environment env;
  REQUIRE(value_of(env, "7") == 7.0);
  REQUIRE(error_kind_of(env, "1 / 0") == ErrorKind::DivideByZero);
  REQUIRE(value_of(env, "ans") == 7.0); // still the last good result
}

TEST_CASE("let binds a name that later lines can use") {
  Environment env;
  REQUIRE(value_of(env, "let x = 5") == 5.0);
  REQUIRE(value_of(env, "x") == 5.0);
  REQUIRE(value_of(env, "x * 2 + 1") == 11.0);
}

TEST_CASE("a let right-hand side is a full expression") {
  Environment env;
  REQUIRE(value_of(env, "let y = 2 + 3 * 4") == 14.0);
  REQUIRE(value_of(env, "sqrt(y + 2)") == 4.0);
}

TEST_CASE("a let can reference earlier names, including itself") {
  Environment env;
  REQUIRE(value_of(env, "let x = 10") == 10.0);
  REQUIRE(value_of(env, "let y = x + 5") == 15.0);
  REQUIRE(value_of(env, "let x = x + 1") ==
          11.0); // reassignment reads the old x
  REQUIRE(value_of(env, "x") == 11.0);
  REQUIRE(value_of(env, "y") == 15.0); // y kept its value
}

TEST_CASE("a let also updates ans") {
  Environment env;
  REQUIRE(value_of(env, "let x = 8") == 8.0);
  REQUIRE(value_of(env, "ans") == 8.0);
}

TEST_CASE("built-in names cant be rebound") {
  Environment env;
  REQUIRE(error_kind_of(env, "let pi = 3") == ErrorKind::ReservedName);
  REQUIRE(error_kind_of(env, "let ans = 1") == ErrorKind::ReservedName);
  REQUIRE(error_kind_of(env, "let sqrt = 2") == ErrorKind::ReservedName);
  REQUIRE(error_kind_of(env, "let MR = 4") == ErrorKind::ReservedName);
}

TEST_CASE("a let with a broken right-hand side reports the error") {
  Environment env;
  REQUIRE(error_kind_of(env, "let x = 1 /") == ErrorKind::UnexpectedToken);
  REQUIRE(error_kind_of(env, "let x = 1 / 0") == ErrorKind::DivideByZero);
  REQUIRE(error_kind_of(env, "let x =") == ErrorKind::EmptyInput);
  REQUIRE(error_kind_of(env, "let = 3") == ErrorKind::UnexpectedToken);
}

TEST_CASE("a broken let doesnt bind the name") {
  Environment env;
  REQUIRE(error_kind_of(env, "let x = 1 / 0") == ErrorKind::DivideByZero);
  REQUIRE(error_kind_of(env, "x") == ErrorKind::UnknownName);
}

TEST_CASE("assignment without let gets a pointed hint") {
  Environment env;
  // the kind is generic, but it shouldnt be a silent success or a crash.
  REQUIRE(error_kind_of(env, "x = 5") == ErrorKind::UnexpectedToken);
}

TEST_CASE("memory recall starts at zero") {
  Environment env;
  REQUIRE(value_of(env, "MR") == 0.0);
  REQUIRE(value_of(env, "M") == 0.0); // M is an alias for recall
}

TEST_CASE("M+ adds the last result to memory") {
  Environment env;
  REQUIRE(value_of(env, "5") == 5.0);
  REQUIRE(value_of(env, "M+") == 5.0); // returns the new register
  REQUIRE(value_of(env, "3") == 3.0);
  REQUIRE(value_of(env, "M+") == 8.0); // accumulates
  REQUIRE(value_of(env, "MR") == 8.0);
}

TEST_CASE("memory recall works inside an expression") {
  Environment env;
  REQUIRE(value_of(env, "10") == 10.0);
  REQUIRE(value_of(env, "M+") == 10.0);
  REQUIRE(value_of(env, "MR + 5") == 15.0);
  REQUIRE(value_of(env, "MR / 2") == 5.0);
}

TEST_CASE("M- subtracts and MC clears") {
  Environment env;
  REQUIRE(value_of(env, "9") == 9.0);
  REQUIRE(value_of(env, "M+") == 9.0);
  REQUIRE(value_of(env, "4") == 4.0);
  REQUIRE(value_of(env, "M-") == 5.0); // 9 - 4
  REQUIRE(value_of(env, "MC") == 0.0);
  REQUIRE(value_of(env, "MR") == 0.0);
}

TEST_CASE("memory commands leave ans alone") {
  Environment env;
  REQUIRE(value_of(env, "6") == 6.0);
  REQUIRE(value_of(env, "M+") == 6.0);
  REQUIRE(value_of(env, "ans") == 6.0); // M+ didnt overwrite ans with memory
}

TEST_CASE("memory mnemonics are case-insensitive") {
  Environment env;
  REQUIRE(value_of(env, "7") == 7.0);
  REQUIRE(value_of(env, "m+") == 7.0);
  REQUIRE(value_of(env, "mr") == 7.0);
  REQUIRE(value_of(env, "mc") == 0.0);
}

TEST_CASE("ans and constants recall regardless of case") {
  Environment env;
  REQUIRE(value_of(env, "2 + 3") == 5.0);
  REQUIRE(value_of(env, "ANS") == 5.0);
  REQUIRE(value_of(env, "Ans") == 5.0);
}

TEST_CASE("an overflowing memory register is caught, not a silent inf") {
  Environment env;
  REQUIRE(value_of(env, "1e308") == 1e308);
  REQUIRE(value_of(env, "M+") == 1e308);
  // a second add pushes the register past what a double can hold; it has to
  // come back as an error, never inf.
  REQUIRE(error_kind_of(env, "M+") == ErrorKind::Overflow);
  REQUIRE(error_kind_of(env, "MR") == ErrorKind::Overflow);
}
