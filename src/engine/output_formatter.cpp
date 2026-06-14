#include "calc/output_formatter.hpp"

#include <cmath>
#include <locale>
#include <sstream>

namespace calc {

std::string format_number(Number value) {
  if (std::isnan(value)) {
    return "nan";
  }
  if (std::isinf(value)) {
    return value < 0 ? "-inf" : "inf";
  }

  // defaultfloat already drops trailing zeros (42.0 -> "42", 3.5 -> "3.5").
  // imbue the classic locale so the decimal point is always '.', matching the
  // lexer, no matter what locale the host process set.
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out.precision(12);
  out << value;
  return out.str();
}

std::string format_result(const Result<Number> &result) {
  if (result) {
    return format_number(result.value());
  }
  return "error: " + result.error().message;
}

} // namespace calc
