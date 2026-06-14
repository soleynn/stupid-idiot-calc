#include "calc/diagnostic.hpp"

#include <cstddef>
#include <string>

#include <fmt/format.h>

namespace calc {

std::string render_diagnostic(std::string_view input, const CalcError &error) {
  const SourceSpan span = error.span;
  if (span.offset == 0 && span.length == 0) {
    return fmt::format("error: {}", error.message); // no location to point at
  }

  std::string out =
      fmt::format("error: {} (column {})\n", error.message, span.offset + 1);
  out.append(input);
  out += '\n';

  // keep tabs in the padding so the caret holds its column at any tab width.
  for (std::size_t i = 0; i < span.offset && i < input.size(); ++i) {
    out += input[i] == '\t' ? '\t' : ' ';
  }
  out += '^';
  for (std::size_t i = 1; i < span.length; ++i) {
    out += '~';
  }
  return out;
}

} // namespace calc
