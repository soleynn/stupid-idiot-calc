#include <catch2/catch_test_macros.hpp>

#include "calc/error.hpp"
#include "calc/number.hpp"
#include "calc/result.hpp"

using namespace calc;

TEST_CASE("Result holds a value") {
  Result<Number> r = Number{42.0};
  REQUIRE(r.has_value());
  REQUIRE(static_cast<bool>(r));
  REQUIRE(r.value() == 42.0);
}

TEST_CASE("Result holds an error") {
  Result<Number> r = CalcError{ErrorKind::DivideByZero, "nope"};
  REQUIRE_FALSE(r.has_value());
  REQUIRE(r.error().kind == ErrorKind::DivideByZero);
  REQUIRE(r.error().message == "nope");
}
