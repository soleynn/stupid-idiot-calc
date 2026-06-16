#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

#include "calc/output_formatter.hpp"

using namespace calc;

// these exact strings are the contract. whatever backs format_number (ostream
// today, fmt next) has to reproduce every one of them byte for byte.
TEST_CASE("format_number output is pinned") {
  struct Case {
    Number value;
    const char *text;
  };
  const double inf = std::numeric_limits<double>::infinity();
  const Case cases[] = {
      {42.0, "42"},
      {0.0, "0"},
      {-0.0, "0"}, // a negative zero folds to a plain "0", not "-0"
      {-42.0, "-42"},
      {2.0, "2"},
      {7.0, "7"},
      {100.0, "100"},
      {1000000.0, "1000000"},
      {3.5, "3.5"},
      {0.5, "0.5"},
      {2.5, "2.5"},
      {-1.5, "-1.5"},
      {0.1, "0.1"},
      {0.0001, "0.0001"},
      {1.0 / 3.0, "0.333333333333"},
      {2.0 / 3.0, "0.666666666667"},
      {12345.6789, "12345.6789"},
      {0.123456789012345, "0.123456789012"},
      {0.000123456789012345, "0.000123456789012"},
      {123456789012.0, "123456789012"},
      {1e11, "100000000000"},
      {1e12, "1e+12"},
      {1e15, "1e+15"},
      {1e20, "1e+20"},
      {1e-5, "1e-05"},
      {1e-20, "1e-20"},
      {1234567890123.0, "1.23456789012e+12"},
      {9999999999999.0, "1e+13"},
      {1e100, "1e+100"},
      {1e-100, "1e-100"},
      {1e308, "1e+308"},
      {inf, "inf"},
      {-inf, "-inf"},
      {std::numeric_limits<double>::quiet_NaN(), "nan"},
  };
  for (const Case &c : cases) {
    CAPTURE(c.text);
    REQUIRE(format_number(c.value) == c.text);
  }
}

TEST_CASE("a negative zero from arithmetic formats as a plain 0") {
  // the kinds of ops that land on -0.0 (a zero times/divided by a negative, a
  // ceil that rounds up to zero) must all read "0", never "-0".
  REQUIRE(format_number(std::copysign(0.0, -1.0)) == "0");
  REQUIRE(format_number(-1.0 * 0.0) == "0");
  REQUIRE(format_number(0.0 / -1.0) == "0");
  REQUIRE(format_number(std::ceil(-0.9)) == "0");
}
