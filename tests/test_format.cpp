#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

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
      {1.0 / 3.0, "0.3333333333333333"},
      {2.0 / 3.0, "0.6666666666666666"},
      {12345.6789, "12345.6789"},
      {0.123456789012345, "0.123456789012345"},
      {0.000123456789012345, "0.000123456789012345"},
      {123456789012.0, "123456789012"},
      {1e11, "100000000000"},
      {1e12, "1000000000000"},
      {1e15, "1000000000000000"},
      {1e20, "1e+20"},
      {1e-5, "1e-05"},
      {1e-20, "1e-20"},
      {1234567890123.0, "1234567890123"},
      {9999999999999.0, "9999999999999"},
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

TEST_CASE("format_number output re-parses to the same double") {
  // the contract: what's shown is the shortest decimal that reads back as the
  // exact same value. these are the cases 12 significant figures used to round
  // off (so "0.3" came back a different double than 0.1 + 0.2 produced).
  const double values[] = {
      0.1 + 0.2,
      1.0 / 3.0,
      2.0 / 3.0,
      1.4142135623730951, // sqrt(2)
      3.141592653589793,  // pi
      0.1,
      0.3,
      1.1,
      12345.6789,
      9999999999999.0,
      123456789012.0,
      1e308,
      1e-100,
      -1.0 / 3.0,
      -(0.1 + 0.2),
  };
  for (const double v : values) {
    CAPTURE(v);
    const std::string shown = format_number(v);
    // strtod is correctly-rounded and always available (unlike from_chars for
    // floats on older libc++), so it re-parses the shown text the same way any
    // conforming reader - including the engine's own lexer - would.
    REQUIRE(std::strtod(shown.c_str(), nullptr) == v);
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
