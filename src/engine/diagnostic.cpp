#include "calc/diagnostic.hpp"

#include <cstddef>
#include <string>

namespace calc {

std::string render_diagnostic(std::string_view input, const CalcError &error) {
  std::string out = "error: " + error.message;

  const SourceSpan span = error.span;
  if (span.offset == 0 && span.length == 0) {
    return out; // no location to point at, e.g. divide by zero
  }

  out += " (column " + std::to_string(span.offset + 1) + ")\n";
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
