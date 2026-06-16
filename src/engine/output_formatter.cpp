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
  // {:.12g} reproduces the old ostream precision(12) output byte for byte.
  return fmt::format("{:.12g}", value);
}

std::string format_result(const Result<Number> &result) {
  if (result) {
    return format_number(result.value());
  }
  return fmt::format("error: {}", result.error().message);
}

} // namespace calc
