#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

#include "calc/output_formatter.hpp"

using namespace calc;

TEST_CASE("format_number trims trailing zeros") {
  REQUIRE(format_number(42.0) == "42");
  REQUIRE(format_number(3.5) == "3.5");
  REQUIRE(format_number(0.0) == "0");
}

TEST_CASE("format_number handles nan and inf") {
  REQUIRE(format_number(std::nan("")) == "nan");
  REQUIRE(format_number(std::numeric_limits<Number>::infinity()) == "inf");
}
