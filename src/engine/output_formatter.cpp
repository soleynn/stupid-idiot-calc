#include "calc/output_formatter.hpp"

#include <cmath>

#include <fmt/format.h>

namespace calc {

std::string format_number(Number value) {
  if (std::isnan(value)) {
    return "nan";
  }
  if (std::isinf(value)) {
    return value < 0 ? "-inf" : "inf";
  }
  // -0.0 compares equal to 0.0 but fmt would still print it as "-0"; fold it to
  // a plain zero so a zero-magnitude result (0 * -1, ceil(-0.9), ...) reads
  // "0".
  if (value == 0.0) {
    value = 0.0;
  }
  // the shortest decimal that round-trips: format_number(x) re-parses to
  // exactly x. fmt's default float format (dragonbox) picks the fewest digits
  // that still pin the value, so 0.5 stays "0.5" but 0.1 + 0.2 shows its true
  // "0.30000000000000004" rather than a rounded "0.3" that re-parses to a
  // different double.
  return fmt::format("{}", value);
}

std::string format_result(const Result<Number> &result) {
  if (result) {
    return format_number(result.value());
  }
  return fmt::format("error: {}", result.error().message);
}

} // namespace calc
