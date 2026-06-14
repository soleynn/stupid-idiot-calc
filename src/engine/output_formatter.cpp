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
